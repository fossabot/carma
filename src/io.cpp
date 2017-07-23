//
// Part of Roadkill Project. Check http://<urlhere> for latest version.
//
// Copyright 2010, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "raiifile.h"
#include "blocks.h"
#include <stdio.h>

// Chunk types
#define FILE_HEADER   0x12

// Chunks in mesh .DAT file
#define MATERIAL_LIST 0x16
#define VERTEX_LIST   0x17
#define UVMAP_LIST    0x18
#define FACE_LIST     0x35
#define FILE_NAME     0x36
#define FACE_MAT_LIST 0x1a

// Chunks in pixelmap .PIX file
#define PIXELMAP_HEAD 0x03
#define PIXELMAP_DATA 0x21

// Chunks in material .MAT file
#define MATERIAL_DESC 0x04
#define PIXELMAP_REF  0x1c
#define RENDERTAB_REF 0x1f

// Chunks in actor .ACT file
#define ACTOR_NAME    0x23  // byte + byte + cstring
#define ACTOR_DATA    0x2b  // position data? 48 bytes (3 groups of 4 floats? trans/rot/scale matrix?)
#define UNKNOWN_NODATA 0x25
#define MATERIAL_REF   0x26
#define UNKNOWN2_NODATA 0x2a // might be tree child/parent specifier? 0x25 goes deeper, 0x2a goes higher? no, not really.
#define MESHFILE_REF  0x24 // cstring

// File types specified in file header:
#define FILE_TYPE_MESH     0xface
#define FILE_TYPE_MATERIAL 0x5
#define FILE_TYPE_PIXELMAP 0x2
#define FILE_TYPE_ACTOR    0x1

//TODO: there are some two-sided materials - find how these are specified (see STIG's spikes and SCREWIE's driveshaft)

using namespace std;
using namespace raii_wrapper;

bool resource_file_t::read_file_header(file& f)
{
    filebinio fio(f);
    chunk_header_t ch;
    CHECK_READ(ch.read(f));
    if (ch.type != FILE_HEADER)
        return false;
    if (ch.size != 8)
        return false;

    uint32_t type, dummy;
    CHECK_READ(fio.read32be(type));
    CHECK_READ(fio.read32be(dummy));

    return true;
}

bool resource_file_t::read_c_string(file& f, std::string& str)
{
    filebinio fio(f);
    str = "";
    int8_t datum = 1;

    while (datum)
    {
        CHECK_READ(fio.read8(datum));
        if (datum)
            str.push_back(toupper(datum));
    }

    return true;
}

bool chunk_header_t::read(file& f)
{
    filebinio fio(f);
    CHECK_READ(fio.read32be(type));
    CHECK_READ(fio.read32be(size));
    return true;
}

// TODO: get rid of this!
bool chunk_t::read(file& f)
{
    filebinio fio(f);
    CHECK_READ(fio.read32be(type));
    CHECK_READ(fio.read32be(size));

    if (type == FILE_NAME) // no entries in this chunk header
    {
        int16_t dummy;
        CHECK_READ(fio.read16be(dummy));
        entries = dummy;
//         size -= 2;
    }
    else
    {
        CHECK_READ(fio.read32be(entries));
//         size -= 4;
    }

    if (type == FACE_MAT_LIST) size += 8;

    return true;
}

static bool read_float32be(file& f, float& val)
{
    filebinio fio(f);
    int32_t datum;
    CHECK_READ(fio.read32be(datum));
    val = *reinterpret_cast<float*>(&datum);
    return true;
}

template <>
bool vector_t<float>::read(file& f)
{
    CHECK_READ(read_float32be(f, x));
    CHECK_READ(read_float32be(f, y));
    CHECK_READ(read_float32be(f, z));
    return true;
}

bool uvcoord_t::read(file& f)
{
    CHECK_READ(read_float32be(f, u));
    CHECK_READ(read_float32be(f, v));
    return true;
}

bool face_t::read(file& f)
{
    filebinio fio(f);
    CHECK_READ(fio.read16be(v1));
    CHECK_READ(fio.read16be(v2));
    CHECK_READ(fio.read16be(v3));
    CHECK_READ(fio.read16be(flags));
    CHECK_READ(fio.read8(unknown));
    return true;
}

bool mesh_t::read(file& f)
{
    vertices.clear();
    uvcoords.clear();
    faces.clear();
    material_names.clear();

    filebinio fio(f);
    chunk_t header;
    chunk_header_t ch;
    uint32_t dummy;
    uint32_t entries;

    printf("Reading filename entry...\n");
    CHECK_READ(header.read(f));

    if (header.type != FILE_NAME)
        return false;

    CHECK_READ(resource_file_t::read_c_string(f, name));

    printf("Reading vertex list...\n");
    CHECK_READ(header.read(f));

    if (header.type != VERTEX_LIST)
        return false;

    for (size_t s = 0; s < header.entries; s++)
    {
        vector_t<float> v;
        CHECK_READ(v.read(f));
        vertices.push_back(v);
    }

    printf("Reading uvmap list...\n");
    CHECK_READ(header.read(f));

    if (header.type != UVMAP_LIST)
        return false;

    for (size_t s = 0; s < header.entries; s++)
    {
        uvcoord_t v;
        CHECK_READ(v.read(f));
        uvcoords.push_back(v);
    }

    printf("Reading face list...\n");
    CHECK_READ(header.read(f));

    if (header.type != FACE_LIST)
        return false;

    for (size_t s = 0; s < header.entries; s++)
    {
        face_t v;
        CHECK_READ(v.read(f));
        faces.push_back(v);
    }

    printf("Reading material list...\n");
    CHECK_READ(ch.read(f));

    if (ch.type == 0 && ch.size == 0) // Some sub-meshes end without defining materials.
        return true;

    if (ch.type != MATERIAL_LIST)
    {
        printf("Expected %x but got type %x (%d bytes) instead!\n", MATERIAL_LIST, ch.type, ch.size);
        return false;
    }

    CHECK_READ(fio.read32be(entries));

    for (size_t s = 0; s < entries; s++)
    {
        string str;
        CHECK_READ(resource_file_t::read_c_string(f, str));
        material_names.push_back(str);
    }

    printf("Reading face material list...\n");
    CHECK_READ(header.read(f));

    if (header.type != FACE_MAT_LIST)
        return false;

    CHECK_READ(fio.read32be(dummy));
    for (size_t s = 0; s < header.entries; s++)
    {
        CHECK_READ(fio.read16be(faces[s].material_id));
    }

    // This is a NULL chunk_header_t marking end of one mesh!
    CHECK_READ(ch.read(f));
    if (!(ch.type == 0 && ch.size == 0))
    {
        printf("Expected terminating chunk but got type %x (%d bytes) instead!\n", ch.type, ch.size);
        return false;
    }

    return true;
}

bool material_t::read(raii_wrapper::file& f)
{
    filebinio fio(f);
    chunk_header_t ch;

    CHECK_READ(ch.read(f));
    if (ch.type != MATERIAL_DESC)
        return false;

    for (int i = 0; i < 12; i++)
    {
        CHECK_READ(read_float32be(f, params[i]));
    }
    CHECK_READ(resource_file_t::read_c_string(f, name));

    CHECK_READ(ch.read(f));

    if (ch.type == 0 && ch.size == 0) // Some materials end without defining pixmaps.
        return true;

    if (ch.type != PIXELMAP_REF)
    {
        printf("Expected %x but got type %x (%d bytes) instead!\n", PIXELMAP_REF, ch.type, ch.size);
        return false;
    }
    CHECK_READ(resource_file_t::read_c_string(f, pixelmap_name));

    CHECK_READ(ch.read(f));
    if (ch.type != RENDERTAB_REF)
    {
        printf("Expected %x but got type %x (%d bytes) instead!\n", RENDERTAB_REF, ch.type, ch.size);
        return false;
    }
    CHECK_READ(resource_file_t::read_c_string(f, rendertab_name));

    // This is a NULL chunk_header_t marking end of one material!
    CHECK_READ(ch.read(f));
    if (!(ch.type == 0 && ch.size == 0))
    {
        printf("Expected terminating chunk but got type %x (%d bytes) instead!\n", ch.type, ch.size);
        return false;
    }

    return true;
}

bool pixelmap_t::read(raii_wrapper::file& f)
{
    filebinio fio(f);
    chunk_header_t ch;

    CHECK_READ(ch.read(f));
    if (ch.type != PIXELMAP_HEAD)
    {
        printf("Expected %x but got type %x (%d bytes) instead!\n", PIXELMAP_HEAD, ch.type, ch.size);
        return false;
    }
    fio.read8(what1);
    fio.read16be(w);
    fio.read16be(use_w);
    fio.read16be(h);
    fio.read16be(use_h);
    fio.read16be(what2);
    CHECK_READ(resource_file_t::read_c_string(f, name));

    CHECK_READ(ch.read(f));
    if (ch.type != PIXELMAP_DATA)
    {
        printf("Expected %x but got type %x (%d bytes) instead!\n", PIXELMAP_DATA, ch.type, ch.size);
        return false;
    }

    fio.read32be(units);
    fio.read32be(unit_bytes);
    uint32_t payload_size = units * unit_bytes;

    data = new uint8_t [payload_size];
    if (!data)
        return false;
    if (f.read(data, payload_size) < payload_size)
        return false;

    // This is a NULL chunk_header_t marking end of one pixmap!
    CHECK_READ(ch.read(f));
    if (!(ch.type == 0 && ch.size == 0))
    {
        printf("Expected terminating chunk but got type %x (%d bytes) instead!\n", ch.type, ch.size);
        return false;
    }

    return true;
}

bool model_t::read(raii_wrapper::file& f)
{
    filebinio fio(f);
    chunk_header_t ch;
    vector_t<float> v;
    actor_t* actor = 0;

    ch.type = 1;
    while (ch.type != 0)
    {
        CHECK_READ(ch.read(f));
        switch (ch.type)
        {
            case ACTOR_NAME:
                // new actor starts
                if (actor)
                    parts[actor->name] = actor;
                actor = new actor_t;
                CHECK_READ(fio.read8(actor->visible));
                CHECK_READ(fio.read8(actor->what2));
                CHECK_READ(resource_file_t::read_c_string(f, actor->name));
                break;
            case ACTOR_DATA:
                CHECK_READ(v.read(f));
                actor->scale.x[0][0] = v.x; actor->scale.x[1][0] = v.y; actor->scale.x[2][0] = v.z;
                CHECK_READ(v.read(f));
                actor->scale.x[0][1] = v.x; actor->scale.x[1][1] = v.y; actor->scale.x[2][1] = v.z;
                CHECK_READ(v.read(f));
                actor->scale.x[0][2] = v.x; actor->scale.x[1][2] = v.y; actor->scale.x[2][2] = v.z;
                CHECK_READ(actor->translate.read(f));
                break;
            case MATERIAL_REF:
                CHECK_READ(resource_file_t::read_c_string(f, actor->material_name));
                break;
            case MESHFILE_REF:
                CHECK_READ(resource_file_t::read_c_string(f, actor->mesh_name));
                break;
            case UNKNOWN_NODATA:
            case UNKNOWN2_NODATA:
                if (ch.size != 0)
                    return false;
            case 0:
                if (actor)
                    parts[actor->name] = actor;
                break;
            default:
                return false;
        }
    }

    if (!(ch.type == 0 && ch.size == 0))
    {
        printf("Expected terminating chunk but got type %x (%d bytes) instead!\n", ch.type, ch.size);
        return false;
    }

    return true;
}