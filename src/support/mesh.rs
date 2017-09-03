//
// Part of Roadkill Project.
//
// Copyright 2010, 2017, Stanislav Karchebnyy <berkus@madfire.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
use byteorder::{BigEndian, ReadBytesExt};
use support::{Error, Vertex};
use support::resource::Chunk;
use std::io::{BufRead, BufReader};
use std::fs::File;
use support;
#[allow(unused_imports)]
use cgmath::{InnerSpace, Vector3, Zero};

#[derive(Copy, Clone, Default)]
pub struct UvCoord {
    u: f32,
    v: f32,
}

impl UvCoord {
    pub fn load<R: ReadBytesExt>(rdr: &mut R) -> Result<UvCoord, Error> {
        let mut uv = UvCoord::default();
        uv.u = rdr.read_f32::<BigEndian>()?;
        uv.v = rdr.read_f32::<BigEndian>()?;
        Ok(uv)
    }
}

#[derive(Default)]
pub struct Face {
    pub v1: u16, // vertex indices (works with glDrawElements() e.g.)
    pub v2: u16,
    pub v3: u16,
    flags: u16, // looks like flags, always only one bit set -- not always, see CITYA81.DAT!!
    material_id: u16, // comes from FACE_MAT_LIST chunk
}

impl Face {
    pub fn load<R: ReadBytesExt>(rdr: &mut R) -> Result<Face, Error> {
        let mut s = Face::default();
        s.v1 = rdr.read_u16::<BigEndian>()?;
        s.v2 = rdr.read_u16::<BigEndian>()?;
        s.v3 = rdr.read_u16::<BigEndian>()?;
        s.flags = rdr.read_u16::<BigEndian>()?;
        rdr.read_i8()?; // something, no idea yet, might be related to flags
        Ok(s)
    }
}

#[derive(Default)]
pub struct Mesh {
    name: String,
    pub vertices: Vec<Vertex>,
    normals: Vec<(f32, f32, f32)>, // calculated normals for each vertex
    uvcoords: Vec<UvCoord>,
    pub faces: Vec<Face>,
    pub material_names: Vec<String>,
}

impl Mesh {
    pub fn load<R: ReadBytesExt + BufRead>(rdr: &mut R) -> Result<Mesh, Error> {
        let mut m = Mesh::default();
        let mut fmlist = Vec::<u16>::new();

        // Read chunks until last chunk is encountered.
        // Certain chunks initialize certain properties.
        loop {
            let c = Chunk::load(rdr)?;
            match c {
                Chunk::FileName(s) => {
                    m.name = s;
                }
                Chunk::VertexList(r) => {
                    m.vertices = r;
                }
                Chunk::UvMapList(r) => {
                    m.uvcoords = r;
                }
                Chunk::FaceList(r) => {
                    m.faces = r;
                }
                Chunk::MaterialList(r) => {
                    m.material_names = r;
                }
                Chunk::FaceMatList(r) => {
                    fmlist = r;
                }
                Chunk::Null() => break,
                Chunk::FileHeader { file_type } => if file_type != support::MESH_FILE_TYPE {
                    panic!("Invalid mesh file type {}", file_type);
                },
                _ => unimplemented!(), // unexpected type here
            }
        }

        if fmlist.len() > 0 && m.faces.len() == fmlist.len() {
            for i in 0..m.faces.len() {
                m.faces[i].material_id = fmlist[i];
            }
        }

        m.calc_normals();
        Ok(m)
    }

    pub fn load_from(fname: String) -> Result<Mesh, Error> {
        let file = File::open(fname)?;
        let mut file = BufReader::new(file);
        Mesh::load(&mut file)
    }

    // Calculate normal from vertices in counter-clockwise order.
    pub fn calc_normal(v1: Vector3<f32>, v2: Vector3<f32>, v3: Vector3<f32>) -> Vector3<f32> {
        (v1 - v2).cross(v2 - v3).normalize()
    }

    pub fn calc_normals(&mut self) {
        self.normals.resize(self.vertices.len(), (0f32, 0f32, 0f32)); // resize_default() in nightly
        for n in 0..self.faces.len() {
            let normal = Mesh::calc_normal(
                Vector3::<f32>::from(self.vertices[self.faces[n].v1 as usize].position),
                Vector3::<f32>::from(self.vertices[self.faces[n].v2 as usize].position),
                Vector3::<f32>::from(self.vertices[self.faces[n].v3 as usize].position),
            );
            self.normals[self.faces[n].v1 as usize] = normal.into();
            self.normals[self.faces[n].v2 as usize] = normal.into();
            self.normals[self.faces[n].v3 as usize] = normal.into();
        }
    }
}

#[cfg(test)]
mod tests {

    use std::io::Cursor;
    use super::*;

    #[test]
    fn test_load_face() {
        let mut data = Cursor::new(vec![0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe, 0]);
        let f = Face::load(&mut data).unwrap();
        assert_eq!(0xdead, f.v1);
        assert_eq!(0xbeef, f.v2);
        assert_eq!(0xcafe, f.v3);
        assert_eq!(0xbabe, f.flags);
    }

    #[test]
    fn test_load_mesh() {
        let mut data = Cursor::new(vec![
            0x0, 0x0, 0x0, 0x36, // Chunk type - FILE_NAME_CHUNK
            0x0, 0x0, 0x0, 0x8, // Chunk size
            0x0, 0x0, // dummy u16
            b'h', b'e', b'l', b'l', b'o', 0, // Chunk contents
            0x0, 0x0, 0x0, 0x0, // Chunk type - NULL_CHUNK
            0x0, 0x0, 0x0, 0x0, // Chunk size
        ]);
        let m = Mesh::load(&mut data).unwrap();
        assert_eq!("hello", m.name);
        // assert_eq!(0xbeef, m.v2);
        // assert_eq!(0xcafe, m.v3);
        // assert_eq!(0xbabe, m.flags);
    }

    // test that normals to unit vectors will be the third unit vector
    #[test]
    fn test_calc_normal() {
        assert_eq!(
            Mesh::calc_normal(Vector3::unit_y(), Vector3::zero(), Vector3::unit_x()),
            Vector3::unit_z()
        );
        assert_eq!(
            Mesh::calc_normal(Vector3::unit_x(), Vector3::zero(), Vector3::unit_y()),
            -Vector3::unit_z()
        );
    }

} // tests mod
