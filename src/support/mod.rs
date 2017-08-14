#![allow(dead_code)]

// extern crate genmesh;
// extern crate obj;

use std;
use std::thread;
use std::time::{Duration, Instant};
use std::io::BufRead;
use std::convert::From;
use byteorder::{BigEndian, ReadBytesExt};
// use glium::{self, Display};
// use glium::vertex::VertexBufferAny;

#[derive(Copy, Clone, Default)]
pub struct Vertex {
    pub position: [f32; 3],
    pub normal: [f32; 3],
    pub tex_coords: [f32; 2],
}

implement_vertex!(Vertex, position, normal, tex_coords);

impl Vertex {
    pub fn load<R: ReadBytesExt>(rdr: &mut R) -> Result<Vertex, Error> {
        let mut vtx = Vertex::default();
        vtx.position[0] = rdr.read_f32::<BigEndian>()?;
        vtx.position[1] = rdr.read_f32::<BigEndian>()?;
        vtx.position[2] = rdr.read_f32::<BigEndian>()?;
        Ok(vtx)
    }
}

pub mod camera;
pub mod material;
pub mod mesh;
pub mod model;
pub mod texture;
pub mod resource;

#[derive(Debug)]
pub enum Error {
    IO(std::io::Error),
    Utf8(std::str::Utf8Error),
    FromUtf8(std::string::FromUtf8Error),
}

impl From<std::io::Error> for Error {
    fn from(error: std::io::Error) -> Self {
        Error::IO(error)
    }
}

impl From<std::str::Utf8Error> for Error {
    fn from(error: std::str::Utf8Error) -> Self {
        Error::Utf8(error)
    }
}

impl From<std::string::FromUtf8Error> for Error {
    fn from(error: std::string::FromUtf8Error) -> Self {
        Error::FromUtf8(error)
    }
}

pub enum Action {
    Stop,
    Continue,
}

pub fn start_loop<F>(mut callback: F) where F: FnMut() -> Action {
    let mut accumulator = Duration::new(0, 0);
    let mut previous_clock = Instant::now();

    loop {
        match callback() {
            Action::Stop => break,
            Action::Continue => ()
        };

        let now = Instant::now();
        accumulator += now - previous_clock;
        previous_clock = now;

        let fixed_time_stamp = Duration::new(0, 16666667);
        while accumulator >= fixed_time_stamp {
            accumulator -= fixed_time_stamp;

            // if you have a game, update the state here
        }

        thread::sleep(fixed_time_stamp - accumulator);
    }
}

// Load a C-style 0-terminated string from the file and return it
pub fn read_c_string<R: BufRead>(reader: &mut R) -> Result<String, Error> {
    let mut buf = vec![];
    /*let num_bytes =*/ reader.read_until(0, &mut buf)?;
    buf.pop();
    let s = String::from_utf8(buf)?;
    Ok(s)
}

// Returns a vertex buffer that should be rendered as `TrianglesList`.
// pub fn load_wavefront(display: &Display, data: &[u8]) -> VertexBufferAny {
//     #[derive(Copy, Clone)]
//     struct Vertex {
//         position: [f32; 3],
//         normal: [f32; 3],
//         texture: [f32; 2],
//     }

//     implement_vertex!(Vertex, position, normal, texture);

//     let mut data = ::std::io::BufReader::new(data);
//     let data = obj::Obj::load(&mut data);

//     let mut vertex_data = Vec::new();

//     for object in data.object_iter() {
//         for shape in object.group_iter().flat_map(|g| g.indices().iter()) {
//             match shape {
//                 &genmesh::Polygon::PolyTri(genmesh::Triangle { x: v1, y: v2, z: v3 }) => {
//                     for v in [v1, v2, v3].iter() {
//                         let position = data.position()[v.0];
//                         let texture = v.1.map(|index| data.texture()[index]);
//                         let normal = v.2.map(|index| data.normal()[index]);

//                         let texture = texture.unwrap_or([0.0, 0.0]);
//                         let normal = normal.unwrap_or([0.0, 0.0, 0.0]);

//                         vertex_data.push(Vertex {
//                             position: position,
//                             normal: normal,
//                             texture: texture,
//                         })
//                     }
//                 },
//                 _ => unimplemented!()
//             }
//         }
//     }

//     glium::vertex::VertexBuffer::new(display, &vertex_data).unwrap().into_vertex_buffer_any()
// }

pub const NULL_CHUNK: u32 = 0x0;
pub const PIXELMAP_HEADER_CHUNK: u32 = 0x3;
pub const MATERIAL_DESC_CHUNK: u32 = 0x4;
pub const FILE_HEADER_CHUNK: u32 = 0x12;
pub const MATERIAL_LIST_CHUNK: u32 = 0x16;
pub const VERTEX_LIST_CHUNK: u32 = 0x17;
pub const UVMAP_LIST_CHUNK: u32 = 0x18;
pub const FACE_MAT_LIST_CHUNK: u32 = 0x1a;
pub const PIXELMAP_REF_CHUNK: u32 = 0x1c;
pub const RENDERTAB_REF_CHUNK: u32 = 0x1f;
pub const PIXELMAP_DATA_CHUNK: u32 = 0x21;
pub const ACTOR_NAME_CHUNK: u32 = 0x23;
pub const UNKNOWN_25_CHUNK: u32 = 0x25;
pub const MESHFILE_REF_CHUNK: u32 = 0x24;
pub const MATERIAL_REF_CHUNK: u32 = 0x26;
pub const UNKNOWN_29_CHUNK: u32 = 0x29;
pub const UNKNOWN_2A_CHUNK: u32 = 0x2a;
pub const ACTOR_TRANSFORM_CHUNK: u32 = 0x2b;
pub const MAP_BOUNDINGBOX_CHUNK: u32 = 0x32;
pub const FACE_LIST_CHUNK: u32 = 0x35;
pub const FILE_NAME_CHUNK: u32 = 0x36;

pub const ACTOR_FILE_TYPE: u32 = 0x1;
pub const PIXELMAP_FILE_TYPE: u32 = 0x2;
pub const MATERIAL_FILE_TYPE: u32 = 0x5;
pub const MESH_FILE_TYPE: u32 = 0xface;

pub const MODEL_FILE_SUBTYPE: u32 = 0x3;

#[cfg(test)]
mod tests {

use std::io::{Cursor, BufReader};
use byteorder::ReadBytesExt;
use super::*;

#[test]
fn test_read_c_string()
{
    let data = Cursor::new(b"hello world\0abc");
    let mut reader = BufReader::new(data);

    let s = read_c_string(&mut reader).unwrap();
    let t = reader.read_u8().unwrap();
    println!("{:?}", t);
    let u = reader.read_u8().unwrap();
    println!("{:?}", u);
    let v = reader.read_u8().unwrap();
    println!("{:?}", v);
    assert_eq!("hello world", s);
    assert_eq!(b"a"[0], t);
    assert_eq!(b"b"[0], u);
    assert_eq!(b"c"[0], v);
}

}
