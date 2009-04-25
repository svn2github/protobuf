// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#ifndef GOOGLE_PROTOBUF_WIRE_FORMAT_INL_H__
#define GOOGLE_PROTOBUF_WIRE_FORMAT_INL_H__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/coded_stream.h>


// Do UTF-8 validation on string type in Debug build only
#ifndef NDEBUG
#define GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
#endif


namespace google {
namespace protobuf {
namespace internal {

inline WireFormat::WireType WireFormat::WireTypeForField(
    const FieldDescriptor* field) {
  if (field->options().packed()) {
    return WIRETYPE_LENGTH_DELIMITED;
  } else {
    return WireTypeForFieldType(field->type());
  }
}

inline uint32 WireFormat::MakeTag(const FieldDescriptor* field) {
  return MakeTag(field->number(), WireTypeForField(field));
}

inline bool WireFormat::ReadInt32(io::CodedInputStream* input, int32* value) {
  uint32 temp;
  if (!input->ReadVarint32(&temp)) return false;
  *value = static_cast<int32>(temp);
  return true;
}
inline bool WireFormat::ReadInt64(io::CodedInputStream* input, int64* value) {
  uint64 temp;
  if (!input->ReadVarint64(&temp)) return false;
  *value = static_cast<int64>(temp);
  return true;
}
inline bool WireFormat::ReadUInt32(io::CodedInputStream* input, uint32* value) {
  return input->ReadVarint32(value);
}
inline bool WireFormat::ReadUInt64(io::CodedInputStream* input, uint64* value) {
  return input->ReadVarint64(value);
}
inline bool WireFormat::ReadSInt32(io::CodedInputStream* input, int32* value) {
  uint32 temp;
  if (!input->ReadVarint32(&temp)) return false;
  *value = ZigZagDecode32(temp);
  return true;
}
inline bool WireFormat::ReadSInt64(io::CodedInputStream* input, int64* value) {
  uint64 temp;
  if (!input->ReadVarint64(&temp)) return false;
  *value = ZigZagDecode64(temp);
  return true;
}
inline bool WireFormat::ReadFixed32(io::CodedInputStream* input,
                                    uint32* value) {
  return input->ReadLittleEndian32(value);
}
inline bool WireFormat::ReadFixed64(io::CodedInputStream* input,
                                    uint64* value) {
  return input->ReadLittleEndian64(value);
}
inline bool WireFormat::ReadSFixed32(io::CodedInputStream* input,
                                     int32* value) {
  uint32 temp;
  if (!input->ReadLittleEndian32(&temp)) return false;
  *value = static_cast<int32>(temp);
  return true;
}
inline bool WireFormat::ReadSFixed64(io::CodedInputStream* input,
                                     int64* value) {
  uint64 temp;
  if (!input->ReadLittleEndian64(&temp)) return false;
  *value = static_cast<int64>(temp);
  return true;
}
inline bool WireFormat::ReadFloat(io::CodedInputStream* input, float* value) {
  uint32 temp;
  if (!input->ReadLittleEndian32(&temp)) return false;
  *value = DecodeFloat(temp);
  return true;
}
inline bool WireFormat::ReadDouble(io::CodedInputStream* input, double* value) {
  uint64 temp;
  if (!input->ReadLittleEndian64(&temp)) return false;
  *value = DecodeDouble(temp);
  return true;
}
inline bool WireFormat::ReadBool(io::CodedInputStream* input, bool* value) {
  uint32 temp;
  if (!input->ReadVarint32(&temp)) return false;
  *value = temp != 0;
  return true;
}
inline bool WireFormat::ReadEnum(io::CodedInputStream* input, int* value) {
  uint32 temp;
  if (!input->ReadVarint32(&temp)) return false;
  *value = static_cast<int>(temp);
  return true;
}

inline bool WireFormat::ReadString(io::CodedInputStream* input, string* value) {
  // String is for UTF-8 text only
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  if (!input->ReadString(value, length)) return false;
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  if (!IsStructurallyValidUTF8(value->data(), length)) {
    GOOGLE_LOG(ERROR) << "Encountered string containing invalid UTF-8 data while "
               "parsing protocol buffer. Strings must contain only UTF-8; "
               "use the 'bytes' type for raw bytes.";
  }
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  return true;
}
inline bool WireFormat::ReadBytes(io::CodedInputStream* input, string* value) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  return input->ReadString(value, length);
}


inline bool WireFormat::ReadGroup(int field_number,
                                  io::CodedInputStream* input,
                                  Message* value) {
  if (!input->IncrementRecursionDepth()) return false;
  if (!value->MergePartialFromCodedStream(input)) return false;
  input->DecrementRecursionDepth();
  // Make sure the last thing read was an end tag for this group.
  if (!input->LastTagWas(MakeTag(field_number, WIRETYPE_END_GROUP))) {
    return false;
  }
  return true;
}
inline bool WireFormat::ReadMessage(io::CodedInputStream* input,
                                    Message* value) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  if (!input->IncrementRecursionDepth()) return false;
  io::CodedInputStream::Limit limit = input->PushLimit(length);
  if (!value->MergePartialFromCodedStream(input)) return false;
  // Make sure that parsing stopped when the limit was hit, not at an endgroup
  // tag.
  if (!input->ConsumedEntireMessage()) return false;
  input->PopLimit(limit);
  input->DecrementRecursionDepth();
  return true;
}

template<typename MessageType>
inline bool WireFormat::ReadGroupNoVirtual(int field_number,
                                           io::CodedInputStream* input,
                                           MessageType* value) {
  if (!input->IncrementRecursionDepth()) return false;
  if (!value->MessageType::MergePartialFromCodedStream(input)) return false;
  input->DecrementRecursionDepth();
  // Make sure the last thing read was an end tag for this group.
  if (!input->LastTagWas(MakeTag(field_number, WIRETYPE_END_GROUP))) {
    return false;
  }
  return true;
}
template<typename MessageType>
inline bool WireFormat::ReadMessageNoVirtual(io::CodedInputStream* input,
                                             MessageType* value) {
  uint32 length;
  if (!input->ReadVarint32(&length)) return false;
  if (!input->IncrementRecursionDepth()) return false;
  io::CodedInputStream::Limit limit = input->PushLimit(length);
  if (!value->MessageType::MergePartialFromCodedStream(input)) return false;
  // Make sure that parsing stopped when the limit was hit, not at an endgroup
  // tag.
  if (!input->ConsumedEntireMessage()) return false;
  input->PopLimit(limit);
  input->DecrementRecursionDepth();
  return true;
}

// ===================================================================

inline void WireFormat::WriteTag(int field_number, WireType type,
                                 io::CodedOutputStream* output) {
  output->WriteTag(MakeTag(field_number, type));
}

inline void WireFormat::WriteInt32NoTag(int32 value,
                                        io::CodedOutputStream* output) {
  output->WriteVarint32SignExtended(value);
}
inline void WireFormat::WriteInt64NoTag(int64 value,
                                        io::CodedOutputStream* output) {
  output->WriteVarint64(static_cast<uint64>(value));
}
inline void WireFormat::WriteUInt32NoTag(uint32 value,
                                         io::CodedOutputStream* output) {
  output->WriteVarint32(value);
}
inline void WireFormat::WriteUInt64NoTag(uint64 value,
                                         io::CodedOutputStream* output) {
  output->WriteVarint64(value);
}
inline void WireFormat::WriteSInt32NoTag(int32 value,
                                         io::CodedOutputStream* output) {
  output->WriteVarint32(ZigZagEncode32(value));
}
inline void WireFormat::WriteSInt64NoTag(int64 value,
                                         io::CodedOutputStream* output) {
  output->WriteVarint64(ZigZagEncode64(value));
}
inline void WireFormat::WriteFixed32NoTag(uint32 value,
                                          io::CodedOutputStream* output) {
  output->WriteLittleEndian32(value);
}
inline void WireFormat::WriteFixed64NoTag(uint64 value,
                                          io::CodedOutputStream* output) {
  output->WriteLittleEndian64(value);
}
inline void WireFormat::WriteSFixed32NoTag(int32 value,
                                           io::CodedOutputStream* output) {
  output->WriteLittleEndian32(static_cast<uint32>(value));
}
inline void WireFormat::WriteSFixed64NoTag(int64 value,
                                           io::CodedOutputStream* output) {
  output->WriteLittleEndian64(static_cast<uint64>(value));
}
inline void WireFormat::WriteFloatNoTag(float value,
                                        io::CodedOutputStream* output) {
  output->WriteLittleEndian32(EncodeFloat(value));
}
inline void WireFormat::WriteDoubleNoTag(double value,
                                         io::CodedOutputStream* output) {
  output->WriteLittleEndian64(EncodeDouble(value));
}
inline void WireFormat::WriteBoolNoTag(bool value,
                                       io::CodedOutputStream* output) {
  output->WriteVarint32(value ? 1 : 0);
}
inline void WireFormat::WriteEnumNoTag(int value,
                                       io::CodedOutputStream* output) {
  output->WriteVarint32SignExtended(value);
}

inline void WireFormat::WriteInt32(int field_number, int32 value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteInt32NoTag(value, output);
}
inline void WireFormat::WriteInt64(int field_number, int64 value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteInt64NoTag(value, output);
}
inline void WireFormat::WriteUInt32(int field_number, uint32 value,
                                    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteUInt32NoTag(value, output);
}
inline void WireFormat::WriteUInt64(int field_number, uint64 value,
                                    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteUInt64NoTag(value, output);
}
inline void WireFormat::WriteSInt32(int field_number, int32 value,
                                    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteSInt32NoTag(value, output);
}
inline void WireFormat::WriteSInt64(int field_number, int64 value,
                                    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteSInt64NoTag(value, output);
}
inline void WireFormat::WriteFixed32(int field_number, uint32 value,
                                     io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteFixed32NoTag(value, output);
}
inline void WireFormat::WriteFixed64(int field_number, uint64 value,
                                     io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteFixed64NoTag(value, output);
}
inline void WireFormat::WriteSFixed32(int field_number, int32 value,
                                      io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteSFixed32NoTag(value, output);
}
inline void WireFormat::WriteSFixed64(int field_number, int64 value,
                                      io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteSFixed64NoTag(value, output);
}
inline void WireFormat::WriteFloat(int field_number, float value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED32, output);
  WriteFloatNoTag(value, output);
}
inline void WireFormat::WriteDouble(int field_number, double value,
                                    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_FIXED64, output);
  WriteDoubleNoTag(value, output);
}
inline void WireFormat::WriteBool(int field_number, bool value,
                                  io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteBoolNoTag(value, output);
}
inline void WireFormat::WriteEnum(int field_number, int value,
                                  io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_VARINT, output);
  WriteEnumNoTag(value, output);
}

inline void WireFormat::WriteString(int field_number, const string& value,
                                    io::CodedOutputStream* output) {
  // String is for UTF-8 text only
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  if (!IsStructurallyValidUTF8(value.data(), value.size())) {
    GOOGLE_LOG(ERROR) << "Encountered string containing invalid UTF-8 data while "
               "serializing protocol buffer. Strings must contain only UTF-8; "
               "use the 'bytes' type for raw bytes.";
  }
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  output->WriteVarint32(value.size());
  output->WriteString(value);
}
inline void WireFormat::WriteBytes(int field_number, const string& value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  output->WriteVarint32(value.size());
  output->WriteString(value);
}


inline void WireFormat::WriteGroup(int field_number, const Message& value,
                                   io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_START_GROUP, output);
  value.SerializeWithCachedSizes(output);
  WriteTag(field_number, WIRETYPE_END_GROUP, output);
}
inline void WireFormat::WriteMessage(int field_number, const Message& value,
                                     io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  output->WriteVarint32(value.GetCachedSize());
  value.SerializeWithCachedSizes(output);
}

template<typename MessageType>
inline void WireFormat::WriteGroupNoVirtual(
    int field_number, const MessageType& value,
    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_START_GROUP, output);
  value.MessageType::SerializeWithCachedSizes(output);
  WriteTag(field_number, WIRETYPE_END_GROUP, output);
}
template<typename MessageType>
inline void WireFormat::WriteMessageNoVirtual(
    int field_number, const MessageType& value,
    io::CodedOutputStream* output) {
  WriteTag(field_number, WIRETYPE_LENGTH_DELIMITED, output);
  output->WriteVarint32(value.MessageType::GetCachedSize());
  value.MessageType::SerializeWithCachedSizes(output);
}

// ===================================================================

inline uint8* WireFormat::WriteTagToArray(int field_number,
                                          WireType type,
                                          uint8* target) {
  return io::CodedOutputStream::WriteTagToArray(MakeTag(field_number, type),
                                                target);
}

inline uint8* WireFormat::WriteInt32NoTagToArray(int32 value, uint8* target) {
  return io::CodedOutputStream::WriteVarint32SignExtendedToArray(value, target);
}
inline uint8* WireFormat::WriteInt64NoTagToArray(int64 value, uint8* target) {
  return io::CodedOutputStream::WriteVarint64ToArray(
      static_cast<uint64>(value), target);
}
inline uint8* WireFormat::WriteUInt32NoTagToArray(uint32 value, uint8* target) {
  return io::CodedOutputStream::WriteVarint32ToArray(value, target);
}
inline uint8* WireFormat::WriteUInt64NoTagToArray(uint64 value, uint8* target) {
  return io::CodedOutputStream::WriteVarint64ToArray(value, target);
}
inline uint8* WireFormat::WriteSInt32NoTagToArray(int32 value, uint8* target) {
  return io::CodedOutputStream::WriteVarint32ToArray(ZigZagEncode32(value),
                                                     target);
}
inline uint8* WireFormat::WriteSInt64NoTagToArray(int64 value, uint8* target) {
  return io::CodedOutputStream::WriteVarint64ToArray(ZigZagEncode64(value),
                                                     target);
}
inline uint8* WireFormat::WriteFixed32NoTagToArray(uint32 value,
                                                   uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian32ToArray(value, target);
}
inline uint8* WireFormat::WriteFixed64NoTagToArray(uint64 value,
                                                   uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian64ToArray(value, target);
}
inline uint8* WireFormat::WriteSFixed32NoTagToArray(int32 value,
                                                    uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian32ToArray(
      static_cast<uint32>(value), target);
}
inline uint8* WireFormat::WriteSFixed64NoTagToArray(int64 value,
                                                    uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian64ToArray(
      static_cast<uint64>(value), target);
}
inline uint8* WireFormat::WriteFloatNoTagToArray(float value, uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian32ToArray(EncodeFloat(value),
                                                           target);
}
inline uint8* WireFormat::WriteDoubleNoTagToArray(double value,
                                                  uint8* target) {
  return io::CodedOutputStream::WriteLittleEndian64ToArray(EncodeDouble(value),
                                                           target);
}
inline uint8* WireFormat::WriteBoolNoTagToArray(bool value,
                                                uint8* target) {
  return io::CodedOutputStream::WriteVarint32ToArray(value ? 1 : 0, target);
}
inline uint8* WireFormat::WriteEnumNoTagToArray(int value,
                                                uint8* target) {
  return io::CodedOutputStream::WriteVarint32SignExtendedToArray(value, target);
}

inline uint8* WireFormat::WriteInt32ToArray(int field_number,
                                            int32 value,
                                            uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteInt32NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteInt64ToArray(int field_number,
                                            int64 value,
                                            uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteInt64NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteUInt32ToArray(int field_number,
                                             uint32 value,
                                             uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteUInt32NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteUInt64ToArray(int field_number,
                                             uint64 value,
                                             uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteUInt64NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteSInt32ToArray(int field_number,
                                             int32 value,
                                             uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteSInt32NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteSInt64ToArray(int field_number,
                                             int64 value,
                                             uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteSInt64NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteFixed32ToArray(int field_number,
                                              uint32 value,
                                              uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED32, target);
  return WriteFixed32NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteFixed64ToArray(int field_number,
                                              uint64 value,
                                              uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED64, target);
  return WriteFixed64NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteSFixed32ToArray(int field_number,
                                               int32 value,
                                               uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED32, target);
  return WriteSFixed32NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteSFixed64ToArray(int field_number,
                                               int64 value,
                                               uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED64, target);
  return WriteSFixed64NoTagToArray(value, target);
}
inline uint8* WireFormat::WriteFloatToArray(int field_number,
                                            float value,
                                            uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED32, target);
  return WriteFloatNoTagToArray(value, target);
}
inline uint8* WireFormat::WriteDoubleToArray(int field_number,
                                             double value,
                                             uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_FIXED64, target);
  return WriteDoubleNoTagToArray(value, target);
}
inline uint8* WireFormat::WriteBoolToArray(int field_number,
                                           bool value,
                                           uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteBoolNoTagToArray(value, target);
}
inline uint8* WireFormat::WriteEnumToArray(int field_number,
                                           int value,
                                           uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_VARINT, target);
  return WriteEnumNoTagToArray(value, target);
}

inline uint8* WireFormat::WriteStringToArray(int field_number,
                                             const string& value,
                                             uint8* target) {
  // String is for UTF-8 text only
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  if (!IsStructurallyValidUTF8(value.data(), value.size())) {
    GOOGLE_LOG(ERROR) << "Encountered string containing invalid UTF-8 data while "
               "serializing protocol buffer. Strings must contain only UTF-8; "
               "use the 'bytes' type for raw bytes.";
  }
#endif
  // WARNING:  In wire_format.cc, both strings and bytes are handled by
  //   WriteString() to avoid code duplication.  If the implementations become
  //   different, you will need to update that usage.
  target = WriteTagToArray(field_number, WIRETYPE_LENGTH_DELIMITED, target);
  target = io::CodedOutputStream::WriteVarint32ToArray(value.size(), target);
  return io::CodedOutputStream::WriteStringToArray(value, target);
}
inline uint8* WireFormat::WriteBytesToArray(int field_number,
                                            const string& value,
                                            uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_LENGTH_DELIMITED, target);
  target = io::CodedOutputStream::WriteVarint32ToArray(value.size(), target);
  return io::CodedOutputStream::WriteStringToArray(value, target);
}


inline uint8* WireFormat::WriteGroupToArray(int field_number,
                                            const Message& value,
                                            uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_START_GROUP, target);
  target = value.SerializeWithCachedSizesToArray(target);
  return WriteTagToArray(field_number, WIRETYPE_END_GROUP, target);
}
inline uint8* WireFormat::WriteMessageToArray(int field_number,
                                              const Message& value,
                                              uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_LENGTH_DELIMITED, target);
  target = io::CodedOutputStream::WriteVarint32ToArray(
    value.GetCachedSize(), target);
  return value.SerializeWithCachedSizesToArray(target);
}

template<typename MessageType>
inline uint8* WireFormat::WriteGroupNoVirtualToArray(
    int field_number, const MessageType& value, uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_START_GROUP, target);
  target = value.MessageType::SerializeWithCachedSizesToArray(target);
  return WriteTagToArray(field_number, WIRETYPE_END_GROUP, target);
}
template<typename MessageType>
inline uint8* WireFormat::WriteMessageNoVirtualToArray(
    int field_number, const MessageType& value, uint8* target) {
  target = WriteTagToArray(field_number, WIRETYPE_LENGTH_DELIMITED, target);
  target = io::CodedOutputStream::WriteVarint32ToArray(
    value.MessageType::GetCachedSize(), target);
  return value.MessageType::SerializeWithCachedSizesToArray(target);
}

// ===================================================================

inline int WireFormat::TagSize(int field_number, FieldDescriptor::Type type) {
  int result = io::CodedOutputStream::VarintSize32(
    field_number << kTagTypeBits);
  if (type == FieldDescriptor::TYPE_GROUP) {
    // Groups have both a start and an end tag.
    return result * 2;
  } else {
    return result;
  }
}

inline int WireFormat::Int32Size(int32 value) {
  return io::CodedOutputStream::VarintSize32SignExtended(value);
}
inline int WireFormat::Int64Size(int64 value) {
  return io::CodedOutputStream::VarintSize64(static_cast<uint64>(value));
}
inline int WireFormat::UInt32Size(uint32 value) {
  return io::CodedOutputStream::VarintSize32(value);
}
inline int WireFormat::UInt64Size(uint64 value) {
  return io::CodedOutputStream::VarintSize64(value);
}
inline int WireFormat::SInt32Size(int32 value) {
  return io::CodedOutputStream::VarintSize32(ZigZagEncode32(value));
}
inline int WireFormat::SInt64Size(int64 value) {
  return io::CodedOutputStream::VarintSize64(ZigZagEncode64(value));
}
inline int WireFormat::EnumSize(int value) {
  return io::CodedOutputStream::VarintSize32SignExtended(value);
}

inline int WireFormat::StringSize(const string& value) {
  return io::CodedOutputStream::VarintSize32(value.size()) +
         value.size();
}
inline int WireFormat::BytesSize(const string& value) {
  return io::CodedOutputStream::VarintSize32(value.size()) +
         value.size();
}


inline int WireFormat::GroupSize(const Message& value) {
  return value.ByteSize();
}
inline int WireFormat::MessageSize(const Message& value) {
  int size = value.ByteSize();
  return io::CodedOutputStream::VarintSize32(size) + size;
}

template<typename MessageType>
inline int WireFormat::GroupSizeNoVirtual(const MessageType& value) {
  return value.MessageType::ByteSize();
}
template<typename MessageType>
inline int WireFormat::MessageSizeNoVirtual(const MessageType& value) {
  int size = value.MessageType::ByteSize();
  return io::CodedOutputStream::VarintSize32(size) + size;
}

}  // namespace internal
}  // namespace protobuf

}  // namespace google
#endif  // GOOGLE_PROTOBUF_WIRE_FORMAT_INL_H__
