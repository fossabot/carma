//
// Part of Roadkill Project. Check http://<urlhere> for latest version.
//
// Copyright 2010, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "raiifile.h"
#include <vector>
#include <map>
#include "math/vector.h"

#define CHECK_READ(v)  if(!(v)) return false

class resource_file_t
{
public:
    /* Helper to read resource file header. */
    static bool read_file_header(raii_wrapper::file& f);
    /* File header itself is a type 0x12 chunk! */

    /* Helper to read C strings */
    static bool read_c_string(raii_wrapper::file& f, std::string& str);
};

// File structures.
class chunk_header_t
{
public:
    uint32_t type;     //chunk type
    uint32_t size;     //size of chunk -4

    bool read(raii_wrapper::file& f);
};

class chunk_t
{
public:
    uint32_t type;     //chunk type
    uint32_t size;     //size of chunk -4
    uint32_t entries;  //number of entires  -- only in DAT, not part of chunk header actually (different for other types)

    bool read(raii_wrapper::file& f);
};

class uvcoord_t
{
public:
    float u, v;

    bool read(raii_wrapper::file& f);
};

class face_t
{
public:
    int16_t v1, v2, v3; // vertex indices (works with glDrawElements() e.g.)
    int16_t flags; // looks like flags, always only one bit set -- not always, see CITYA81.DAT!!
    int8_t unknown; // something, no idea yet, might be related to flags
    int16_t material_id;

    bool read(raii_wrapper::file& f);
};

// Materials.
// MAT file is an index of: material internal name, PIX file name and TAB file name.
class material_t
{
public:
    float params[12];
    std::string name;
    std::string pixelmap_name;
    std::string rendertab_name;

    bool read(raii_wrapper::file& f);
    void dump();
};

// Mesh.
class mesh_t
{
public:
    std::string name;
    std::vector<vector_t<float> > vertices;
    std::vector<vector_t<float> > normals; // calculated normals for each vertex
    std::vector<uvcoord_t> uvcoords;
    std::vector<face_t> faces;
    std::vector<std::string> material_names;
    std::map<std::string, material_t> materials;

    bool read(raii_wrapper::file& f);
    void calc_normals();
    void dump();
};

// Pixelmaps.
// Pixmap consists of two chunks: name and data

class pixelmap_t
{
public:
    std::string name;
    uint16_t w, h, use_w, use_h; /* Actual texture w & h and how much of that is used for useful data */
    uint8_t what1;
    uint16_t what2;
    uint32_t units, unit_bytes;
    uint8_t* data;

    pixelmap_t() : w(0), h(0), use_w(0), use_h(0), data(0) {}
    pixelmap_t(const pixelmap_t& other);
    pixelmap_t& operator =(const pixelmap_t& other);
    ~pixelmap_t() { delete data; }
    bool read(raii_wrapper::file& f);
    void dump();
};

// Actors.
// Actors group multiple meshes into a single car body with pivots, shafts and wheels.

class actor_t
{
public:
    uint8_t visible, what2;
    std::string name;
    float values[12]; // 1 matrix? 3 quaternions (wxyz)? 4 vectors?
    std::string material_name;
    std::string mesh_name;

    void dump();
};

class model_t
{
public:
    std::map<std::string, actor_t*> parts;
    bool read(raii_wrapper::file& f);
    void dump();
};
