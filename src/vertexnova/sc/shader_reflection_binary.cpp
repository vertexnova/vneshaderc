/* ---------------------------------------------------------------------
 * Copyright (c) 2026 Ajeet Singh Yadav. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Author:    Ajeet Singh Yadav
 * Created:   May 2026
 *
 * Autodoc:   yes
 * ----------------------------------------------------------------------
 */

#include "vertexnova/sc/shader_reflection_binary.h"

#include <sstream>

namespace {

constexpr uint32_t kReflectionBinaryVersion = 2u;  // bumped: BackendSlot -> ResourceBackendSlots
constexpr uint32_t kMaxStringLen = 4096;
constexpr uint32_t kMaxProgramStageCount = 16;
constexpr uint32_t kMaxBindingCount = 256;
constexpr uint32_t kMaxStructMemberCount = 256;

struct Writer {
    std::ostringstream os{std::ios::binary};
    void u8(uint8_t v) { os.write(reinterpret_cast<const char*>(&v), 1); }
    void u32(uint32_t v) { os.write(reinterpret_cast<const char*>(&v), 4); }
    void boolean(bool v) { u8(static_cast<uint8_t>(v ? 1 : 0)); }
    void str(const std::string& s) {
        u32(static_cast<uint32_t>(s.size()));
        os.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
};

struct Reader {
    std::istringstream is;
    const size_t data_size;

    explicit Reader(const std::string& data)
        : is(data, std::ios::binary)
        , data_size(data.size()) {}

    size_t remaining() {
        const auto pos = is.tellg();
        if (pos < 0) {
            return 0;
        }
        return data_size - static_cast<size_t>(pos);
    }

    void fail() { is.setstate(std::ios::failbit); }

    void requireBytes(size_t n) {
        if (!ok()) {
            return;
        }
        if (remaining() < n) {
            fail();
        }
    }

    uint8_t u8() {
        if (!ok()) {
            return 0;
        }
        requireBytes(1);
        if (!ok()) {
            return 0;
        }
        uint8_t v{};
        is.read(reinterpret_cast<char*>(&v), 1);
        if (is.gcount() != 1) {
            fail();
        }
        return v;
    }

    uint32_t u32() {
        if (!ok()) {
            return 0;
        }
        requireBytes(4);
        if (!ok()) {
            return 0;
        }
        uint32_t v{};
        is.read(reinterpret_cast<char*>(&v), 4);
        if (is.gcount() != 4) {
            fail();
        }
        return v;
    }

    uint32_t boundedCount(uint32_t max_count) {
        const uint32_t count = u32();
        if (!ok()) {
            return 0;
        }
        if (count > max_count) {
            fail();
            return 0;
        }
        return count;
    }

    bool boolean() { return u8() != 0u; }
    std::string str() {
        if (!ok()) {
            return {};
        }
        const uint32_t len = u32();
        if (!ok()) {
            return {};
        }
        if (len > kMaxStringLen) {
            fail();
            return {};
        }
        requireBytes(len);
        if (!ok()) {
            return {};
        }
        std::string s(len, '\0');
        if (len > 0) {
            is.read(s.data(), static_cast<std::streamsize>(len));
            if (static_cast<uint32_t>(is.gcount()) != len) {
                fail();
                return {};
            }
        }
        return s;
    }

    bool ok() const { return !is.fail(); }
};

void writeReflectedStructMember(Writer& w, const vne::sc::ReflectedStructMember& m) {
    w.str(m.name);
    w.u32(m.offset);
    w.u32(m.size);
    w.u32(m.array_count);
    w.u32(m.array_stride);
    w.boolean(m.is_matrix);
    w.u32(m.matrix_columns);
    w.u32(m.matrix_rows);
    w.str(m.type_name);
}

bool readReflectedStructMember(Reader& r, vne::sc::ReflectedStructMember& m) {
    if (!r.ok()) {
        return false;
    }
    m.name = r.str();
    if (!r.ok()) {
        return false;
    }
    m.offset = r.u32();
    m.size = r.u32();
    m.array_count = r.u32();
    m.array_stride = r.u32();
    if (!r.ok()) {
        return false;
    }
    m.is_matrix = r.boolean();
    m.matrix_columns = r.u32();
    m.matrix_rows = r.u32();
    if (!r.ok()) {
        return false;
    }
    m.type_name = r.str();
    return r.ok();
}

void writeResourceBackendSlots(Writer& w, const vne::sc::ResourceBackendSlots& s) {
    w.boolean(s.metal.has_value());
    if (s.metal) {
        w.u32(s.metal->buffer);
        w.u32(s.metal->texture);
        w.u32(s.metal->sampler);
    }
    w.boolean(s.webgpu.has_value());
    if (s.webgpu) {
        w.u32(s.webgpu->group);
        w.u32(s.webgpu->binding);
    }
}

bool readResourceBackendSlots(Reader& r, vne::sc::ResourceBackendSlots& s) {
    if (!r.ok()) {
        return false;
    }
    s = {};
    const bool has_metal = r.boolean();
    if (has_metal) {
        vne::sc::MetalResourceSlot metal;
        metal.buffer = r.u32();
        metal.texture = r.u32();
        metal.sampler = r.u32();
        if (!r.ok()) {
            return false;
        }
        s.metal = metal;
    }
    const bool has_webgpu = r.boolean();
    if (has_webgpu) {
        vne::sc::WebGpuResourceSlot webgpu;
        webgpu.group = r.u32();
        webgpu.binding = r.u32();
        if (!r.ok()) {
            return false;
        }
        s.webgpu = webgpu;
    }
    return r.ok();
}

void writeReflectedBindingInfo(Writer& w, const vne::sc::ReflectedBindingInfo& b) {
    w.str(b.name);
    w.u8(static_cast<uint8_t>(b.type));
    w.u32(b.set);
    w.u32(b.binding);
    w.u32(b.array_size);
    w.u32(static_cast<uint32_t>(b.stages));
    writeResourceBackendSlots(w, b.slots);
    w.u32(static_cast<uint32_t>(b.struct_members.size()));
    for (const auto& m : b.struct_members) {
        writeReflectedStructMember(w, m);
    }
}

bool readReflectedBindingInfo(Reader& r, vne::sc::ReflectedBindingInfo& b) {
    if (!r.ok()) {
        return false;
    }
    b.name = r.str();
    if (!r.ok()) {
        return false;
    }
    b.type = static_cast<vne::sc::ReflectedResourceType>(r.u8());
    b.set = r.u32();
    b.binding = r.u32();
    b.array_size = r.u32();
    b.stages = static_cast<vne::sc::ShaderStageFlags>(r.u32());
    if (!r.ok() || !readResourceBackendSlots(r, b.slots)) {
        return false;
    }
    const uint32_t member_count = r.boundedCount(kMaxStructMemberCount);
    if (!r.ok()) {
        return false;
    }
    b.struct_members.resize(member_count);
    for (auto& m : b.struct_members) {
        if (!readReflectedStructMember(r, m)) {
            return false;
        }
    }
    return r.ok();
}

void writeStageReflection(Writer& w, const vne::sc::StageReflection& sr) {
    w.u8(static_cast<uint8_t>(sr.stage));
    w.u32(static_cast<uint32_t>(sr.bindings.size()));
    for (const auto& b : sr.bindings) {
        writeReflectedBindingInfo(w, b);
    }
    w.u32(sr.push_constant_size);
    w.u32(sr.workgroup_size.x);
    w.u32(sr.workgroup_size.y);
    w.u32(sr.workgroup_size.z);
}

bool readStageReflection(Reader& r, vne::sc::StageReflection& sr) {
    if (!r.ok()) {
        return false;
    }
    sr.stage = static_cast<vne::sc::ShaderStage>(r.u8());
    const uint32_t binding_count = r.boundedCount(kMaxBindingCount);
    if (!r.ok()) {
        return false;
    }
    sr.bindings.resize(binding_count);
    for (auto& b : sr.bindings) {
        if (!readReflectedBindingInfo(r, b)) {
            return false;
        }
    }
    sr.push_constant_size = r.u32();
    sr.workgroup_size.x = r.u32();
    sr.workgroup_size.y = r.u32();
    sr.workgroup_size.z = r.u32();
    return r.ok();
}

}  // namespace

namespace vne::sc {

std::string serializeStageReflection(const StageReflection& reflection) {
    Writer w;
    writeStageReflection(w, reflection);
    return w.os.str();
}

bool deserializeStageReflection(const std::string& data, StageReflection& out) {
    Reader r(data);
    if (!readStageReflection(r, out)) {
        return false;
    }
    return r.ok();
}

std::string serializeProgramReflection(const ProgramReflection& reflection) {
    Writer w;
    w.u32(kReflectionBinaryVersion);
    w.u32(static_cast<uint32_t>(reflection.stages.size()));
    for (const auto& stage : reflection.stages) {
        writeStageReflection(w, stage);
    }
    return w.os.str();
}

bool deserializeProgramReflection(const std::string& data, ProgramReflection& out) {
    Reader r(data);
    const uint32_t version = r.u32();
    if (!r.ok() || version != kReflectionBinaryVersion) {
        return false;
    }
    const uint32_t count = r.boundedCount(kMaxProgramStageCount);
    if (!r.ok()) {
        return false;
    }
    out.stages.resize(count);
    for (auto& stage : out.stages) {
        if (!readStageReflection(r, stage)) {
            return false;
        }
    }
    return r.ok();
}

}  // namespace vne::sc
