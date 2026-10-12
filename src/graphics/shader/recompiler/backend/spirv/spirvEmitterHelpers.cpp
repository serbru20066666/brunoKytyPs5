#include "graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h"

#include <cstdlib>

namespace Libs::Graphics::ShaderRecompiler::Spirv::Emitter {

DppTargetLane EmitDppPermTargetLane(EmitterState& state, uint32_t subid, uint32_t control,
                                    uint32_t lane_bits) {
	const auto lane_mask  = (1u << lane_bits) - 1u;
	const auto group_base = state.builder.AllocateId();
	const auto lane       = state.builder.AllocateId();
	const auto shift      = state.builder.AllocateId();
	const auto selected0  = state.builder.AllocateId();
	const auto selected   = state.builder.AllocateId();
	const auto target     = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), group_base, subid,
	                          ConstantU32(state, ~lane_mask));
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid,
	                          ConstantU32(state, lane_mask));
	state.builder.AddFunction(spv::OpIMul, TypeU32(state), shift, lane,
	                          ConstantU32(state, lane_bits));
	state.builder.AddFunction(spv::OpShiftRightLogical, TypeU32(state), selected0,
	                          ConstantU32(state, control), shift);
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), selected, selected0,
	                          ConstantU32(state, lane_mask));
	state.builder.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, group_base, selected);
	return {target, ConstantBool(state, true)};
}

DppTargetLane EmitDppRowShiftTargetLane(EmitterState& state, uint32_t subid, uint32_t amount,
                                        bool left) {
	const auto row          = state.builder.AllocateId();
	const auto lane         = state.builder.AllocateId();
	const auto lane_shifted = state.builder.AllocateId();
	const auto target       = state.builder.AllocateId();
	const auto valid        = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), row, subid,
	                          ConstantU32(state, 0xfffffff0u));
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid,
	                          ConstantU32(state, 15));
	if (left) {
		state.builder.AddFunction(spv::OpIAdd, TypeU32(state), lane_shifted, lane,
		                          ConstantU32(state, amount));
		state.builder.AddFunction(spv::OpULessThan, TypeBool(state), valid, lane,
		                          ConstantU32(state, 16u - amount));
	} else {
		state.builder.AddFunction(spv::OpISub, TypeU32(state), lane_shifted, lane,
		                          ConstantU32(state, amount));
		state.builder.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), valid, lane,
		                          ConstantU32(state, amount));
	}
	state.builder.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, row, lane_shifted);
	return {target, valid};
}

DppTargetLane EmitDppRowRotateRightTargetLane(EmitterState& state, uint32_t subid,
                                              uint32_t amount) {
	const auto row      = state.builder.AllocateId();
	const auto lane     = state.builder.AllocateId();
	const auto in_high  = state.builder.AllocateId();
	const auto minus    = state.builder.AllocateId();
	const auto plus     = state.builder.AllocateId();
	const auto selected = state.builder.AllocateId();
	const auto target   = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), row, subid,
	                          ConstantU32(state, 0xfffffff0u));
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid,
	                          ConstantU32(state, 15));
	state.builder.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), in_high, lane,
	                          ConstantU32(state, amount));
	state.builder.AddFunction(spv::OpISub, TypeU32(state), minus, lane, ConstantU32(state, amount));
	state.builder.AddFunction(spv::OpIAdd, TypeU32(state), plus, lane,
	                          ConstantU32(state, 16u - amount));
	state.builder.AddFunction(spv::OpSelect, TypeU32(state), selected, in_high, minus, plus);
	state.builder.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, row, selected);
	return {target, ConstantBool(state, true)};
}

DppTargetLane EmitDppMirrorTargetLane(EmitterState& state, uint32_t subid, bool half_row) {
	const auto base_mask = half_row ? 0xfffffff8u : 0xfffffff0u;
	const auto lane_mask = half_row ? 7u : 15u;
	const auto base      = state.builder.AllocateId();
	const auto lane      = state.builder.AllocateId();
	const auto mirrored  = state.builder.AllocateId();
	const auto target    = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), base, subid,
	                          ConstantU32(state, base_mask));
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane, subid,
	                          ConstantU32(state, lane_mask));
	state.builder.AddFunction(spv::OpISub, TypeU32(state), mirrored, ConstantU32(state, lane_mask),
	                          lane);
	state.builder.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, base, mirrored);
	return {target, ConstantBool(state, true)};
}

DppTargetLane EmitDppTargetLane(EmitterState& state, const IR::DppMoveFlags& flags) {
	const auto subid   = EmitSubgroupLocalInvocationId(state);
	const auto control = flags.control;
	if (flags.dpp8) {
		return EmitDppPermTargetLane(state, subid, control, 3u);
	}
	if (control <= 0xffu) {
		return EmitDppPermTargetLane(state, subid, control, 2u);
	}
	if (control >= 0x101u && control <= 0x10fu) {
		return EmitDppRowShiftTargetLane(state, subid, control & 0xfu, true);
	}
	if (control >= 0x111u && control <= 0x11fu) {
		return EmitDppRowShiftTargetLane(state, subid, control & 0xfu, false);
	}
	if (control >= 0x121u && control <= 0x12fu) {
		return EmitDppRowRotateRightTargetLane(state, subid, control & 0xfu);
	}
	if (control == 0x140u) {
		return EmitDppMirrorTargetLane(state, subid, false);
	}
	if (control == 0x141u) {
		return EmitDppMirrorTargetLane(state, subid, true);
	}
	// row_share:N reads lane N of the caller's row of 16.
	if (control >= 0x150u && control <= 0x15fu) {
		const auto row    = state.builder.AllocateId();
		const auto target = state.builder.AllocateId();
		state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), row, subid,
		                          ConstantU32(state, 0xfffffff0u));
		state.builder.AddFunction(spv::OpBitwiseOr, TypeU32(state), target, row,
		                          ConstantU32(state, control & 0xfu));
		return {target, ConstantBool(state, true)};
	}
	if (control >= 0x160u && control <= 0x16fu) {
		const auto target = state.builder.AllocateId();
		state.builder.AddFunction(spv::OpBitwiseXor, TypeU32(state), target, subid,
		                          ConstantU32(state, control & 0xfu));
		return {target, ConstantBool(state, true)};
	}
	return {subid, ConstantBool(state, true)};
}

bool WaveHalvesInHostSubgroup(const EmitterState& state) {
	// KYTY_DEBUG_WAVE_HALVES=0 emits as if the host subgroup matched the guest wave (A/B).
	static const bool enabled = [] {
		const char* text = std::getenv("KYTY_DEBUG_WAVE_HALVES");
		return text == nullptr || std::strcmp(text, "0") != 0;
	}();
	const auto stage = state.program.stage;
	return enabled && state.program.wave_size == 32u && state.lane_count == 1 &&
	       (stage == ShaderType::Vertex || stage == ShaderType::Local ||
	        stage == ShaderType::TessellationEvaluation);
}

uint32_t EmitOwnWaveHalfBase(EmitterState& state) {
	return EmitBinaryU32(state, spv::OpBitwiseAnd, EmitSubgroupLocalInvocationId(state),
	                     ConstantU32(state, 32));
}

uint32_t EmitOwnWaveHalfWord(EmitterState& state, uint32_t ballot) {
	const auto low   = state.builder.AllocateId();
	const auto high  = state.builder.AllocateId();
	const auto upper = state.builder.AllocateId();
	const auto word  = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, ballot, 0);
	state.builder.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, ballot, 1);
	state.builder.AddFunction(spv::OpINotEqual, TypeBool(state), upper, EmitOwnWaveHalfBase(state),
	                          ConstantU32(state, 0));
	state.builder.AddFunction(spv::OpSelect, TypeU32(state), word, upper, high, low);
	return word;
}

uint32_t EmitLaunchedLaneAtOrBelow(EmitterState& state, uint32_t lane) {
	// KYTY_DEBUG_HELPER_LANES=1 (experiment, no measured effect): helper invocations do not
	// count as launched.
	uint32_t predicate = ConstantBool(state, true);
	if (state.helper_invocation_variable != 0 && std::getenv("KYTY_DEBUG_HELPER_LANES") != nullptr) {
		const auto helper = state.builder.AllocateId();
		state.builder.AddFunction(spv::OpLoad, TypeBool(state), helper,
		                          state.helper_invocation_variable);
		predicate = EmitLogicalNotBool(state, helper);
	}
	const auto ballot = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpGroupNonUniformBallot, TypeU32Vector(state, 4), ballot,
	                          ConstantU32(state, spv::ScopeSubgroup), predicate);
	const auto low      = state.builder.AllocateId();
	const auto in_row   = EmitBinaryU32(state, spv::OpBitwiseAnd, lane, ConstantU32(state, 31));
	const auto excluded = EmitBinaryU32(state, spv::OpShiftLeftLogical,
	                                    ConstantU32(state, 0xfffffffeu), in_row);
	const auto below    = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, ballot, 0);
	state.builder.AddFunction(spv::OpNot, TypeU32(state), below, excluded);
	uint32_t word = 0;
	uint32_t base = 0;
	if (state.program.wave_size == 64u && state.lane_count == 1) {
		// A search from the high word falls back to the low one when no high lane launched.
		const auto high      = state.builder.AllocateId();
		const auto in_high   = state.builder.AllocateId();
		const auto high_hit  = state.builder.AllocateId();
		const auto use_high  = state.builder.AllocateId();
		const auto low_word  = state.builder.AllocateId();
		state.builder.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, ballot, 1);
		const auto high_below = EmitBinaryU32(state, spv::OpBitwiseAnd, high, below);
		state.builder.AddFunction(
		    spv::OpINotEqual, TypeBool(state), in_high,
		    EmitBinaryU32(state, spv::OpBitwiseAnd, lane, ConstantU32(state, 32)),
		    ConstantU32(state, 0));
		state.builder.AddFunction(spv::OpINotEqual, TypeBool(state), high_hit, high_below,
		                          ConstantU32(state, 0));
		state.builder.AddFunction(spv::OpLogicalAnd, TypeBool(state), use_high, in_high, high_hit);
		state.builder.AddFunction(spv::OpSelect, TypeU32(state), low_word, in_high, low,
		                          EmitBinaryU32(state, spv::OpBitwiseAnd, low, below));
		word = state.builder.AllocateId();
		base = state.builder.AllocateId();
		state.builder.AddFunction(spv::OpSelect, TypeU32(state), word, use_high, high_below,
		                          low_word);
		state.builder.AddFunction(spv::OpSelect, TypeU32(state), base, use_high,
		                          ConstantU32(state, 32), ConstantU32(state, 0));
	} else {
		// One word: a wave32 in its own subgroup, the invocation's own half of a 64-lane one, or
		// both halves of a wave64 emulated two lanes per invocation (launched together).
		const auto launched = WaveHalvesInHostSubgroup(state) ? EmitOwnWaveHalfWord(state, ballot)
		                                                      : low;
		word = EmitBinaryU32(state, spv::OpBitwiseAnd, launched, below);
		base = state.program.wave_size == 64u || WaveHalvesInHostSubgroup(state)
		           ? EmitBinaryU32(state, spv::OpBitwiseAnd, lane, ConstantU32(state, 32))
		           : ConstantU32(state, 0);
	}
	const auto highest = state.builder.AllocateId();
	const auto any     = state.builder.AllocateId();
	const auto result  = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpExtInst, TypeU32(state), highest, GlslStd450(state),
	                          GLSLstd450FindUMsb, word);
	const auto launched = EmitAddU32(state, base, highest);
	state.builder.AddFunction(spv::OpINotEqual, TypeBool(state), any, word, ConstantU32(state, 0));
	state.builder.AddFunction(spv::OpSelect, TypeU32(state), result, any, launched, lane);
	return result;
}

bool ShuffleByReduction(const EmitterState& state) {
	// KYTY_DEBUG_PIXEL_SHUFFLE=1 keeps OpGroupNonUniformShuffle in pixel shaders.
	static const bool keep_shuffle = std::getenv("KYTY_DEBUG_PIXEL_SHUFFLE") != nullptr;
	return state.program.stage == ShaderType::Pixel && state.program.info.subgroup_shuffle &&
	       !keep_shuffle;
}

// AMD's shader compiler (32.0.21045.5002, RX 6800 XT) dies with an access violation building a
// pixel shader of EA SPORTS FC 27 that shuffles inside nested loops; the same module compiles
// once every shuffle is a reduction. In pixel shaders a lane's value is therefore read as the OR
// over the subgroup of "am I that lane ? value : 0". An inactive lane then reads as zero, where
// a shuffle's result is undefined.
uint32_t EmitSubgroupShuffle(EmitterState& state, uint32_t type, uint32_t value, uint32_t lane) {
	const auto scope   = ConstantU32(state, spv::ScopeSubgroup);
	const auto result  = state.builder.AllocateId();
	const bool word    = type == TypeU32(state);
	const bool boolean = type == TypeBool(state);
	if (!ShuffleByReduction(state) || (!word && !boolean) ||
	    state.subgroup_local_invocation_id_variable == 0) {
		state.builder.AddFunction(spv::OpGroupNonUniformShuffle, type, result, scope, value, lane);
		return result;
	}
	// KYTY_DEBUG_SHUFFLE_BALLOT=1: the lane's value from ballots alone, one per bit.
	if (static const bool by_ballot = std::getenv("KYTY_DEBUG_SHUFFLE_BALLOT") != nullptr;
	    by_ballot) {
		const auto word_index =
		    Binary(state, spv::OpShiftRightLogical, TypeU32(state), lane, ConstantU32(state, 5));
		const auto bit_index =
		    Binary(state, spv::OpBitwiseAnd, TypeU32(state), lane, ConstantU32(state, 31));
		const auto lane_bit = [&](uint32_t predicate) {
			const auto ballot = state.builder.AllocateId();
			state.builder.AddFunction(spv::OpGroupNonUniformBallot, TypeU32Vector(state, 4), ballot,
			                          scope, predicate);
			const auto ballot_word = state.builder.AllocateId();
			state.builder.AddFunction(spv::OpVectorExtractDynamic, TypeU32(state), ballot_word,
			                          ballot, word_index);
			return Binary(state, spv::OpBitwiseAnd, TypeU32(state),
			              Binary(state, spv::OpShiftRightLogical, TypeU32(state), ballot_word,
			                     bit_index),
			              ConstantU32(state, 1));
		};
		if (boolean) {
			state.builder.AddFunction(spv::OpINotEqual, type, result, lane_bit(value),
			                          ConstantU32(state, 0));
			return result;
		}
		uint32_t built = ConstantU32(state, 0);
		for (uint32_t bit = 0; bit < 32; bit++) {
			const auto set = Binary(
			    state, spv::OpINotEqual, TypeBool(state),
			    Binary(state, spv::OpBitwiseAnd, TypeU32(state), value, ConstantU32(state, 1u << bit)),
			    ConstantU32(state, 0));
			built = Binary(state, spv::OpBitwiseOr, TypeU32(state), built,
			               Binary(state, spv::OpShiftLeftLogical, TypeU32(state), lane_bit(set),
			                      ConstantU32(state, bit)));
		}
		state.builder.AddFunction(spv::OpCopyObject, type, result, built);
		return result;
	}
	// The host's own invocation index: `lane` is one too (no guest half offset).
	const auto own = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpLoad, TypeU32(state), own,
	                          state.subgroup_local_invocation_id_variable);
	const auto is_lane = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpIEqual, TypeBool(state), is_lane, own, lane);
	const auto picked = state.builder.AllocateId();
	if (word) {
		state.builder.AddFunction(spv::OpSelect, type, picked, is_lane, value,
		                          ConstantU32(state, 0));
		state.builder.AddFunction(spv::OpGroupNonUniformBitwiseOr, type, result, scope,
		                          spv::GroupOperationReduce, picked);
	} else {
		state.builder.AddFunction(spv::OpLogicalAnd, type, picked, is_lane, value);
		state.builder.AddFunction(spv::OpGroupNonUniformLogicalOr, type, result, scope,
		                          spv::GroupOperationReduce, picked);
	}
	return result;
}

uint32_t EmitSubgroupLocalInvocationId(EmitterState& state) {
	if (state.subgroup_local_invocation_id_variable == 0) {
		EXIT("SubgroupLocalInvocationId was not declared before SPIR-V function emission\n");
	}
	const auto value = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpLoad, TypeU32(state), value,
	                          state.subgroup_local_invocation_id_variable);
	return state.lane_half == 0 ? value : EmitAddU32(state, value, ConstantU32(state, 32));
}

uint32_t InputVariableForKind(const EmitterState& state, IR::StageInputKind kind) {
	for (const auto& input: state.inputs) {
		if (input.kind == kind) {
			return input.variable_id;
		}
	}
	return 0;
}

const InputBinding* InputBindingForParameter(const EmitterState& state, uint32_t location) {
	for (const auto& input: state.inputs) {
		if (input.kind == IR::StageInputKind::Parameter && input.location == location) {
			return &input;
		}
	}
	return nullptr;
}

uint32_t EmitInputComponentU32(EmitterState& state, IR::StageInputKind kind, uint32_t component) {
	const auto variable = InputVariableForKind(state, kind);
	if (variable == 0) {
		return ConstantU32(state, 0);
	}
	const auto pointer = state.builder.AllocateId();
	const auto value   = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpAccessChain,
	                          TypePointer(state, spv::StorageClassInput, TypeU32(state)), pointer,
	                          variable, ConstantU32(state, component));
	state.builder.AddFunction(spv::OpLoad, TypeU32(state), value, pointer);
	return value;
}

uint32_t EmitLocalInvocationIndex(EmitterState& state) {
	const auto variable = InputVariableForKind(state, IR::StageInputKind::LocalInvocationIndex);
	if (variable == 0) {
		return ConstantU32(state, 0);
	}
	const auto value = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpLoad, TypeU32(state), value, variable);
	if (state.lane_count == 2) {
		const auto wave_base =
		    EmitBinaryU32(state, spv::OpBitwiseAnd, value, ConstantU32(state, ~31u));
		return EmitAddU32(state, EmitAddU32(state, value, wave_base),
		                  ConstantU32(state, state.lane_half * 32));
	}
	return value;
}

uint32_t EmitVertexParameterComponentU32(EmitterState& state, const InputBinding& input,
                                         uint32_t component) {
	const auto count       = VertexParameterComponentCount(input);
	const auto kind        = VertexParameterScalarKind(state, input.location);
	const auto scalar_type = VertexParameterScalarType(state, kind);
	uint32_t   raw         = state.builder.AllocateId();
	if (count == 1u) {
		state.builder.AddFunction(spv::OpLoad, scalar_type, raw, input.variable_id);
	} else {
		const auto pointer_type = TypePointer(state, spv::StorageClassInput, scalar_type);
		const auto pointer      = state.builder.AllocateId();
		state.builder.AddFunction(spv::OpAccessChain, pointer_type, pointer, input.variable_id,
		                          ConstantU32(state, component));
		state.builder.AddFunction(spv::OpLoad, scalar_type, raw, pointer);
	}

	if (kind == VertexInputScalarKind::Uint) {
		return raw;
	}

	const auto bits = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpBitcast, TypeU32(state), bits, raw);
	return bits;
}

uint32_t EmitSubgroupLaneActiveBool(EmitterState& state, uint32_t lane) {
	uint32_t predicate = ConstantBool(state, true);
	if (state.helper_invocation_variable != 0) {
		const auto helper = state.builder.AllocateId();
		state.builder.AddFunction(spv::OpLoad, TypeBool(state), helper,
		                          state.helper_invocation_variable);
		predicate = EmitLogicalNotBool(state, helper);
	}
	const auto active_ballot = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpGroupNonUniformBallot, TypeU32Vector(state, 4), active_ballot,
	                          ConstantU32(state, spv::ScopeSubgroup), predicate);
	return EmitBallotLaneActiveBool(state, active_ballot, lane);
}
uint32_t EmitBallotLaneActiveBool(EmitterState& state, uint32_t active_ballot, uint32_t lane) {
	const auto low = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpCompositeExtract, TypeU32(state), low, active_ballot, 0);
	uint32_t mask = low;
	if (state.program.wave_size == 64u) {
		const auto high     = state.builder.AllocateId();
		const auto in_high  = state.builder.AllocateId();
		const auto selected = state.builder.AllocateId();
		state.builder.AddFunction(spv::OpCompositeExtract, TypeU32(state), high, active_ballot, 1);
		state.builder.AddFunction(spv::OpUGreaterThanEqual, TypeBool(state), in_high, lane,
		                          ConstantU32(state, 32));
		state.builder.AddFunction(spv::OpSelect, TypeU32(state), selected, in_high, high, low);
		mask = selected;
	}

	const auto lane_low = state.builder.AllocateId();
	const auto bit      = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), lane_low, lane,
	                          ConstantU32(state, 31));
	state.builder.AddFunction(spv::OpShiftLeftLogical, TypeU32(state), bit, ConstantU32(state, 1),
	                          lane_low);

	const auto hit      = state.builder.AllocateId();
	const auto active   = state.builder.AllocateId();
	const auto in_range = state.builder.AllocateId();
	const auto ret      = state.builder.AllocateId();
	state.builder.AddFunction(spv::OpBitwiseAnd, TypeU32(state), hit, mask, bit);
	state.builder.AddFunction(spv::OpINotEqual, TypeBool(state), active, hit,
	                          ConstantU32(state, 0));
	state.builder.AddFunction(spv::OpULessThan, TypeBool(state), in_range, lane,
	                          ConstantU32(state, state.program.wave_size));
	state.builder.AddFunction(spv::OpLogicalAnd, TypeBool(state), ret, active, in_range);
	return ret;
}

} // namespace Libs::Graphics::ShaderRecompiler::Spirv::Emitter
