// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.
// http://code.google.com/p/protobuf/
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <algorithm>

#include <google/protobuf/compiler/importer.h>

#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {

#ifdef _WIN32
#ifndef F_OK
#define F_OK 00  // not defined by MSVC for whatever reason
#endif
#endif

MultiFileErrorCollector::~MultiFileErrorCollector() {}

// This class serves two purposes:
// - It implements the ErrorCollector interface (used by Tokenizer and Parser)
//   in terms of MultiFileErrorCollector, using a particular filename.
// - It lets us check if any errors have occurred.
class SourceTreeDescriptorDatabase::SingleFileErrorCollector
    : public io::ErrorCollector {
 public:
  SingleFileErrorCollector(const string& filename,
                           MultiFileErrorCollector* multi_file_error_collector)
    : filename_(filename),
      multi_file_error_collector_(multi_file_error_collector),
      had_errors_(false) {}
  ~SingleFileErrorCollector() {}

  bool had_errors() { return had_errors_; }

  // implements ErrorCollector ---------------------------------------
  void AddError(int line, int column, const string& message) {
    if (multi_file_error_collector_ != NULL) {
      multi_file_error_collector_->AddError(filename_, line, column, message);
    }
    had_errors_ = true;
  }

 private:
  string filename_;
  MultiFileErrorCollector* multi_file_error_collector_;
  bool had_errors_;
};

// ===================================================================

SourceTreeDescriptorDatabase::SourceTreeDescriptorDatabase(
    SourceTree* source_tree)
  : source_tree_(source_tree),
    error_collector_(NULL),
    using_validation_error_collector_(false),
    validation_error_collector_(this) {}

SourceTreeDescriptorDatabase::~SourceTreeDescriptorDatabase() {}

bool SourceTreeDescriptorDatabase::FindFileByName(
    const string& filename, FileDescriptorProto* output) {
  scoped_ptr<io::ZeroCopyInputStream> input(source_tree_->Open(filename));
  if (input == NULL) {
    if (error_collector_ != NULL) {
      error_collector_->AddError(filename, -1, 0, "File not found.");
    }
    return false;
  }

  // Set up the tokenizer and parser.
  SingleFileErrorCollector file_error_collector(filename, error_collector_);
  io::Tokenizer tokenizer(input.get(), &file_error_collector);

  Parser parser;
  if (error_collector_ != NULL) {
    parser.RecordErrorsTo(&file_error_collector);
  }
  if (using_validation_error_collector_) {
    parser.RecordSourceLocationsTo(&source_locations_);
  }

  // Parse it.
  output->set_name(filename);
  return parser.Parse(&tokenizer, output) &&
         !file_error_collector.had_errors();
}

bool SourceTreeDescriptorDatabase::FindFileContainingSymbol(
    const string& symbol_name, FileDescriptorProto* output) {
  return false;
}

bool SourceTreeDescriptorDatabase::FindFileContainingExtension(
    const string& containing_type, int field_number,
    FileDescriptorProto* output) {
  return false;
}

// -------------------------------------------------------------------

SourceTreeDescriptorDatabase::ValidationErrorCollector::
ValidationErrorCollector(SourceTreeDescriptorDatabase* owner)
  : owner_(owner) {}

SourceTreeDescriptorDatabase::ValidationErrorCollector::
~ValidationErrorCollector() {}

void SourceTreeDescriptorDatabase::ValidationErrorCollector::AddError(
    const string& filename,
    const string& element_name,
    const Message* descriptor,
    ErrorLocation location,
    const string& message) {
  if (owner_->error_collector_ == NULL) return;

  int line, column;
  owner_->source_locations_.Find(descriptor, location, &line, &column);
  owner_->error_collector_->AddError(filename, line, column, message);
}

// ===================================================================

Importer::Importer(SourceTree* source_tree,
                   MultiFileErrorCollector* error_collector)
  : database_(source_tree),
    pool_(&database_, database_.GetValidationErrorCollector()) {
  database_.RecordErrorsTo(error_collector);
}

Importer::~Importer() {}

const FileDescriptor* Importer::Import(const string& filename) {
  return pool_.FindFileByName(filename);
}

// ===================================================================

SourceTree::~SourceTree() {}

DiskSourceTree::DiskSourceTree() {}

DiskSourceTree::~DiskSourceTree() {}

static inline char LastChar(const string& str) {
  return str[str.size() - 1];
}

// Given a path, returns an equivalent path with these changes:
// - On Windows, any backslashes are replaced with forward slashes.
// - Any instances of the directory "." are removed.
// - Any consecutive '/'s are collapsed into a single slash.
// Note that the resulting string may be empty.
//
// TODO(kenton):  It would be nice to handle "..", e.g. so that we can figure
//   out that "foo/bar.proto" is inside "baz/../foo".  However, if baz is a
//   symlink or doesn't exist, then things get complicated, and we can't
//   actually determine this without investigating the filesystem, probably
//   in non-portable ways.  So, we punt.
//
// TODO(kenton):  It would be nice to use realpath() here except that it
//   resolves symbolic links.  This could cause problems if people place
//   symbolic links in their source tree.  For example, if you executed:
//     protoc --proto_path=foo foo/bar/baz.proto
//   then if foo/bar is a symbolic link, foo/bar/baz.proto will canonicalize
//   to a path which does not appear to be under foo, and thus the compiler
//   will complain that baz.proto is not inside the --proto_path.
static string CanonicalizePath(string path) {
#ifdef _WIN32
  // The Win32 API accepts forward slashes as a path delimiter even though
  // backslashes are standard.  Let's avoid confusion and use only forward
  // slashes.
  path = StringReplace(path, "\\", "/", true);
#endif

  vector<string> parts;
  vector<string> canonical_parts;
  SplitStringUsing(path, "/", &parts);  // Note:  Removes empty parts.
  for (int i = 0; i < parts.size(); i++) {
    if (parts[i] == ".") {
      // Ignore.
    } else {
      canonical_parts.push_back(parts[i]);
    }
  }
  string result = JoinStrings(canonical_parts, "/");
  if (!path.empty() && path[0] == '/') {
    // Restore leading slash.
    result = '/' + result;
  }
  if (!path.empty() && LastChar(path) == '/' &&
      !result.empty() && LastChar(result) != '/') {
    // Restore trailing slash.
    result += '/';
  }
  return result;
}

static inline bool ContainsParentReference(const string& path) {
  return path == ".." ||
         HasPrefixString(path, "../") ||
         HasSuffixString(path, "/..") ||
         path.find("/../") != string::npos;
}

// Maps a file from an old location to a new one.  Typically, old_prefix is
// a virtual path and new_prefix is its corresponding disk path.  Returns
// false if the filename did not start with old_prefix, otherwise replaces
// old_prefix with new_prefix and stores the result in *result.  Examples:
//   string result;
//   assert(ApplyMapping("foo/bar", "", "baz", &result));
//   assert(result == "baz/foo/bar");
//
//   assert(ApplyMapping("foo/bar", "foo", "baz", &result));
//   assert(result == "baz/bar");
//
//   assert(ApplyMapping("foo", "foo", "bar", &result));
//   assert(result == "bar");
//
//   assert(!ApplyMapping("foo/bar", "baz", "qux", &result));
//   assert(!ApplyMapping("foo/bar", "baz", "qux", &result));
//   assert(!ApplyMapping("foobar", "foo", "baz", &result));
static bool ApplyMapping(const string& filename,
                         const string& old_prefix,
                         const string& new_prefix,
                         string* result) {
  if (old_prefix.empty()) {
    // old_prefix matches any relative path.
    if (ContainsParentReference(filename)) {
      // We do not allow the file name to use "..".
      return false;
    }
    if (HasPrefixString(filename, "/")) {
      // This is an absolute path, so it isn't matched by the empty string.
      return false;
    }
    result->assign(new_prefix);
    if (!result->empty()) result->push_back('/');
    result->append(filename);
    return true;
  } else if (HasPrefixString(filename, old_prefix)) {
    // old_prefix is a prefix of the filename.  Is it the whole filename?
    if (filename.size() == old_prefix.size()) {
      // Yep, it's an exact match.
      *result = new_prefix;
      return true;
    } else {
      // Not an exact match.  Is the next character a '/'?  Otherwise,
      // this isn't actually a match at all.  E.g. the prefix "foo/bar"
      // does not match the filename "foo/barbaz".
      int after_prefix_start = -1;
      if (filename[old_prefix.size()] == '/') {
        after_prefix_start = old_prefix.size() + 1;
      } else if (filename[old_prefix.size() - 1] == '/') {
        // old_prefix is never empty, and canonicalized paths never have
        // consecutive '/' characters.
        after_prefix_start = old_prefix.size();
      }
      if (after_prefix_start != -1) {
        // Yep.  So the prefixes are directories and the filename is a file
        // inside them.
        string after_prefix = filename.substr(after_prefix_start);
        if (ContainsParentReference(after_prefix)) {
          // We do not allow the file name to use "..".
          return false;
        }
        result->assign(new_prefix);
        if (!result->empty()) result->push_back('/');
        result->append(after_prefix);
        return true;
      }
    }
  }

  return false;
}

void DiskSourceTree::MapPath(const string& virtual_path,
                             const string& disk_path) {
  mappings_.push_back(Mapping(virtual_path, CanonicalizePath(disk_path)));
}

DiskSourceTree::DiskFileToVirtualFileResult
DiskSourceTree::DiskFileToVirtualFile(
    const string& disk_file,
    string* virtual_file,
    string* shadowing_disk_file) {
  int mapping_index = -1;
  string canonical_disk_file = CanonicalizePath(disk_file);

  for (int i = 0; i < mappings_.size(); i++) {
    // Apply the mapping in reverse.
    if (ApplyMapping(canonical_disk_file, mappings_[i].disk_path,
                     mappings_[i].virtual_path, virtual_file)) {
      // Success.
      mapping_index = i;
      break;
    }
  }

  if (mapping_index == -1) {
    return NO_MAPPING;
  }

  // Iterate through all mappings with higher precedence and verify that none
  // of them map this file to some other existing file.
  for (int i = 0; i < mapping_index; i++) {
    if (ApplyMapping(*virtual_file, mappings_[i].virtual_path,
                     mappings_[i].disk_path, shadowing_disk_file)) {
      if (access(shadowing_disk_file->c_str(), F_OK) >= 0) {
        // File exists.
        return SHADOWED;
      }
    }
  }
  shadowing_disk_file->clear();

  // Verify that we can open the file.  Note that this also has the side-effect
  // of verifying that we are not canonicalizing away any non-existent
  // directories.
  scoped_ptr<io::ZeroCopyInputStream> stream(OpenDiskFile(disk_file));
  if (stream == NULL) {
    return CANNOT_OPEN;
  }

  return SUCCESS;
}

io::ZeroCopyInputStream* DiskSourceTree::Open(const string& filename) {
  if (filename != CanonicalizePath(filename) ||
      ContainsParentReference(filename)) {
    // We do not allow importing of paths containing things like ".." or
    // consecutive slashes since the compiler expects files to be uniquely
    // identified by file name.
    return NULL;
  }

  for (int i = 0; i < mappings_.size(); i++) {
    string disk_file;
    if (ApplyMapping(filename, mappings_[i].virtual_path,
                     mappings_[i].disk_path, &disk_file)) {
      io::ZeroCopyInputStream* stream = OpenDiskFile(disk_file);
      if (stream != NULL) return stream;

      if (errno == EACCES) {
        // The file exists but is not readable.
        // TODO(kenton):  Find a way to report this more nicely.
        GOOGLE_LOG(WARNING) << "Read access is denied for file: " << disk_file;
        return NULL;
      }
    }
  }

  return NULL;
}

io::ZeroCopyInputStream* DiskSourceTree::OpenDiskFile(
    const string& filename) {
  int file_descriptor;
  do {
    file_descriptor = open(filename.c_str(), O_RDONLY);
  } while (file_descriptor < 0 && errno == EINTR);
  if (file_descriptor >= 0) {
    io::FileInputStream* result = new io::FileInputStream(file_descriptor);
    result->SetCloseOnDelete(true);
    return result;
  } else {
    return NULL;
  }
}

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
