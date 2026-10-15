#pragma once

#include "libs/ajm/decoder.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include <opus_multistream.h>

namespace Libs::Audio::Ajm {

struct AjmDecOpusInitializeParameters {
	uint32_t channel_num;
	uint32_t sample_rate;
	uint32_t mapping_family;
};

struct AjmSidebandDecOpusCodecInfo {
	uint32_t frames_per_packet;
};

static_assert(sizeof(AjmDecOpusInitializeParameters) == 12);
static_assert(sizeof(AjmSidebandDecOpusCodecInfo) == 4);

class AjmOpusDecoder final: public AjmDecoder {
public:
	AjmOpusDecoder(uint32_t max_channels, AjmSampleEncoding encoding)
	    : AjmDecoder(0, 48000, encoding), m_max_channels(max_channels) {}

	~AjmOpusDecoder() override { Destroy(); }

	AjmDecodeResult Initialize(const void* parameters, size_t parameters_size) override {
		Destroy();
		Reset();
		auto result = MakeResult();
		if (parameters == nullptr || parameters_size < sizeof(AjmDecOpusInitializeParameters)) {
			result.result = AJM_RESULT_INVALID_PARAMETER | AJM_RESULT_FATAL;
			return result;
		}
		const auto& params = *static_cast<const AjmDecOpusInitializeParameters*>(parameters);
		if (static const bool trace = std::getenv("KYTY_DEBUG_AJM") != nullptr; trace) {
			std::printf("ajm: opus initialize size=%zu channels=%u rate=%u mapping=%u max=%u\n",
			            parameters_size, params.channel_num, params.sample_rate,
			            params.mapping_family, m_max_channels);
			std::fflush(stdout);
		}
		const bool family_ok = params.mapping_family == 0 || params.mapping_family == 1 ||
		                       params.mapping_family == 255;
		if (params.channel_num == 0 || params.channel_num > 8 || params.sample_rate != 48000 ||
		    !family_ok || (params.mapping_family == 0 && params.channel_num > 2)) {
			result.result = AJM_RESULT_INVALID_PARAMETER | AJM_RESULT_FATAL;
			return result;
		}
		if (params.channel_num > m_max_channels) {
			result.result = AJM_RESULT_TOO_MANY_CHANNELS | AJM_RESULT_FATAL;
			return result;
		}

		struct Layout {
			int           streams;
			int           coupled;
			unsigned char mapping[8];
		};
		static constexpr Layout VORBIS_LAYOUTS[8] = {
		    {1, 0, {0}},
		    {1, 1, {0, 1}},
		    {2, 1, {0, 2, 1}},
		    {2, 2, {0, 1, 2, 3}},
		    {3, 2, {0, 4, 1, 2, 3}},
		    {4, 2, {0, 4, 1, 2, 3, 5}},
		    {4, 3, {0, 4, 1, 2, 3, 5, 6}},
		    {5, 3, {0, 6, 1, 2, 3, 4, 5, 7}},
		};
		// Family 255 carries one uncoupled mono stream per channel, in channel order.
		static constexpr Layout INDEPENDENT_LAYOUTS[8] = {
		    {1, 0, {0}},
		    {2, 0, {0, 1}},
		    {3, 0, {0, 1, 2}},
		    {4, 0, {0, 1, 2, 3}},
		    {5, 0, {0, 1, 2, 3, 4}},
		    {6, 0, {0, 1, 2, 3, 4, 5}},
		    {7, 0, {0, 1, 2, 3, 4, 5, 6}},
		    {8, 0, {0, 1, 2, 3, 4, 5, 6, 7}},
		};
		const auto& layout = params.mapping_family == 255
		                         ? INDEPENDENT_LAYOUTS[params.channel_num - 1]
		                         : VORBIS_LAYOUTS[params.channel_num - 1];
		int         error  = OPUS_OK;
		m_decoder = opus_multistream_decoder_create(48000, static_cast<int>(params.channel_num),
		                                            layout.streams, layout.coupled,
		                                            layout.mapping, &error);
		if (m_decoder == nullptr || error != OPUS_OK) {
			Destroy();
			result.result = AJM_RESULT_CODEC_ERROR | AJM_RESULT_FATAL;
			return result;
		}
		SetFormat(params.channel_num, params.sample_rate, m_sample_encoding);
		return MakeResult();
	}

	void Reset() override {
		if (m_decoder != nullptr) {
			opus_multistream_decoder_ctl(m_decoder, OPUS_RESET_STATE);
		}
		m_total_decoded_samples = 0;
		m_frames_per_packet     = 0;
	}

	AjmDecodeResult Decode(const void* input, size_t input_size, void* output, size_t output_size,
	                       bool multiple_frames, AjmGaplessState* gapless) override {
		auto result = MakeResult();
		if (m_decoder == nullptr) {
			result.result = AJM_RESULT_NOT_INITIALIZED;
			return result;
		}
		if (input == nullptr || input_size == 0) {
			result.result = AJM_RESULT_PARTIAL_INPUT;
			return result;
		}

		while (gapless == nullptr || !gapless->IsEnd()) {
			const auto* data      = static_cast<const uint8_t*>(input) + result.input_consumed;
			const auto  remaining = input_size - result.input_consumed;
			if (remaining < 2) {
				result.result = AJM_RESULT_PARTIAL_INPUT;
				break;
			}
			const auto packet_size = static_cast<uint32_t>(data[0]) | (uint32_t {data[1]} << 8u);
			if (packet_size > remaining - 2) {
				result.result = AJM_RESULT_PARTIAL_INPUT;
				break;
			}
			data += 2;
			const int frames_in = packet_size != 0 ? opus_packet_get_nb_frames(data, packet_size)
			                                       : OPUS_INVALID_PACKET;
			const int samples_in =
			    packet_size != 0 ? opus_packet_get_nb_samples(data, packet_size, 48000)
			                     : OPUS_INVALID_PACKET;
			if (frames_in <= 0 || samples_in <= 0) {
				result.result          = AJM_RESULT_CODEC_ERROR | AJM_RESULT_INVALID_DATA;
				result.internal_result = 0x1043;
				break;
			}
			const auto samples = static_cast<uint32_t>(samples_in);
			const auto frames  = static_cast<uint32_t>(frames_in);
			const auto skip    = gapless != nullptr
			                         ? std::min(samples, uint32_t {gapless->current.skip_samples})
			                         : 0;
			const auto write   = gapless != nullptr && gapless->HasSampleLimit()
			                         ? std::min(samples - skip, gapless->current.total_samples)
			                         : samples - skip;
			const auto sample_bytes = m_channels * AjmBytesPerSample(m_sample_encoding);
			const auto output_bytes = write * sample_bytes;
			if (output_bytes > output_size - result.output_written ||
			    (output_bytes != 0 && output == nullptr)) {
				result.result = AJM_RESULT_NOT_ENOUGH_ROOM;
				break;
			}

			m_pcm.resize(static_cast<size_t>(samples) * m_channels);
			const int decoded = opus_multistream_decode_float(
			    m_decoder, data, static_cast<opus_int32>(packet_size), m_pcm.data(),
			    static_cast<int>(samples), 0);
			if (decoded != static_cast<int>(samples)) {
				result.result          = AJM_RESULT_CODEC_ERROR | AJM_RESULT_INVALID_DATA;
				result.internal_result = 0x1043;
				break;
			}
			if (output_bytes != 0) {
				StorePcm(static_cast<uint8_t*>(output) + result.output_written,
				         m_pcm.data() + static_cast<size_t>(skip) * m_channels,
				         static_cast<size_t>(write) * m_channels);
			}
			if (gapless != nullptr) {
				gapless->current.skip_samples -= static_cast<uint16_t>(skip);
				gapless->current.skipped_samples = static_cast<uint16_t>(
				    std::min<uint32_t>(std::numeric_limits<uint16_t>::max(),
				                       gapless->current.skipped_samples + samples - write));
				if (gapless->HasSampleLimit()) {
					gapless->current.total_samples -= write;
				}
			}
			result.input_consumed += packet_size + 2;
			result.output_written += output_bytes;
			result.frames += frames;
			m_frames_per_packet = frames;
			m_total_decoded_samples += write;
			if (!multiple_frames) {
				break;
			}
		}
		result.frames_per_packet     = m_frames_per_packet;
		result.total_decoded_samples = m_total_decoded_samples;
		return result;
	}

	void WriteCodecInfo(void* output, size_t size, const AjmDecodeResult&) const override {
		if (output != nullptr && size >= sizeof(AjmSidebandDecOpusCodecInfo)) {
			static_cast<AjmSidebandDecOpusCodecInfo*>(output)->frames_per_packet =
			    m_frames_per_packet;
		}
	}

	[[nodiscard]] size_t CodecInfoSize() const override {
		return sizeof(AjmSidebandDecOpusCodecInfo);
	}

private:
	void Destroy() {
		if (m_decoder != nullptr) {
			opus_multistream_decoder_destroy(m_decoder);
			m_decoder = nullptr;
		}
	}

	static float SoftLimit(float x) {
		if (x > 0.8f) {
			return 0.8f + 0.2f * std::tanh((x - 0.8f) / 0.2f);
		}
		if (x < -0.8f) {
			return -(0.8f + 0.2f * std::tanh((-x - 0.8f) / 0.2f));
		}
		return x;
	}

	static float GetOpusGain() {
		static const float gain = [] {
			if (const char* env = std::getenv("KYTY_DEBUG_OPUS_GAIN"); env != nullptr && env[0] != '\0') {
				return std::strtof(env, nullptr);
			}
			return 2.2f;
		}();
		return gain;
	}

	void StorePcm(uint8_t* dst, const float* src, size_t count) const {
		const float gain = GetOpusGain();
		switch (m_sample_encoding) {
			case AjmSampleEncoding::Float:
				if (gain == 1.0f) {
					std::memcpy(dst, src, count * sizeof(float));
				} else {
					auto* out = reinterpret_cast<float*>(dst);
					for (size_t i = 0; i < count; i++) {
						out[i] = SoftLimit(src[i] * gain);
					}
				}
				break;
			case AjmSampleEncoding::S16:
				for (size_t i = 0; i < count; i++) {
					const auto v = static_cast<int16_t>(
					    std::lrint(SoftLimit(src[i] * gain) * 32767.0f));
					std::memcpy(dst + i * sizeof(v), &v, sizeof(v));
				}
				break;
			default:
				for (size_t i = 0; i < count; i++) {
					const auto v = static_cast<int32_t>(
					    std::llrint(SoftLimit(src[i] * gain) * 2147483647.0));
					std::memcpy(dst + i * sizeof(v), &v, sizeof(v));
				}
				break;
		}
	}

	OpusMSDecoder*     m_decoder = nullptr;
	const uint32_t     m_max_channels;
	uint32_t           m_frames_per_packet = 0;
	std::vector<float> m_pcm;
};

}
