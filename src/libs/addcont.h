#ifndef EMULATOR_INCLUDE_EMULATOR_LIBS_ADDCONT_H_
#define EMULATOR_INCLUDE_EMULATOR_LIBS_ADDCONT_H_

#include "common/common.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Libs {

// Add-on content - the console's addcont - is kept beside the emulator, one directory per add-on:
//
//     Content\<title id>\<folder>\
//         sce_sys\param.json
//
// A folder answers to one entitlement label: the last dash-separated part of the contentId in its
// param.json, which is the sixteen characters the guest asks about, or its own name when there is no
// readable param.json. The mount in libAppContent and the entitlement list in libNet answer from
// this one pairing, so a folder cannot mount under a label the list calls unowned, and a label that
// matches nothing names the directory to rename instead of taking another add-on's files.
namespace Addcont {

// The guest's entitlement label field holds sixteen characters, so a folder's label is cut to those
// below: everything that compares a label - the mount, the list, the single-label question - works
// with the same sixteen.
constexpr size_t LABEL_SIZE = 16;

struct Folder {
	std::string           label;
	std::filesystem::path path;
};

// Content\<title id>\, where the add-ons of a title are looked for.
std::filesystem::path Directory(const std::string& title_id);

// The add-ons of that directory, each with the label it answers to, in name order.
std::vector<Folder> Folders(const std::string& title_id);

} // namespace Addcont

} // namespace Libs

#endif /* EMULATOR_INCLUDE_EMULATOR_LIBS_ADDCONT_H_ */
