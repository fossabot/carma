//
// Part of Roadkill Project.
//
// Copyright 2010, 2017, Stanislav Karchebnyy <berkus@madfire.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
use support::Error;
use std::io::{BufRead, BufReader};
use std::fs::File;
use byteorder::ReadBytesExt;
use support::resource::Chunk;
use support;
use id_tree::*;

// Typical actor tree:
// Root
// +--Actor(NAME.ACT)
//    +--Transform()
//    +--MeshfileRef
//    +--Actor(SUBACT.ACT)
//       +--Transform()
//       +--MeshfileRef
//
#[derive(Debug)]
pub enum ActorNode {
    Root,
    Actor { name: String, visible: bool },
    // First 3x3 is scale? or maybe SQT?
    // Last 3 is translate, -x is to the left, -z is to the front
    Transform([f32; 12]),
    MeshfileRef(String),
    MaterialRef(String),
}

pub struct Actor {
    pub tree: Tree<ActorNode>,
    pub root_id: NodeId,
}

impl Actor {
    pub fn new(tree: Tree<ActorNode>) -> Self {
        let mut tree = tree;
        let root_id = tree.insert(Node::new(ActorNode::Root), InsertBehavior::AsRoot).unwrap();
        Self {
            tree: tree,
            root_id: root_id,
        }
    }

    pub fn dump(&self) {
        for node in self.tree.traverse_pre_order(&self.root_id).unwrap() {
            if let Some(parent) = node.parent() {
                print!("  ");
                for _ in self.tree.ancestors(parent).unwrap() {
                    print!("  ");
                }
            }
            println!("{:?}", node.data());
        }
    }

    pub fn load<R: ReadBytesExt + BufRead>(rdr: &mut R) -> Result<Actor, Error> {
        use id_tree::InsertBehavior::*;

        let mut actor = Actor::new(TreeBuilder::new()
                .with_node_capacity(5)
                .build());

        {
        let mut current_actor = actor.root_id.clone();
        let mut last_actor = current_actor.clone();

        // Read chunks until last chunk is encountered.
        // Certain chunks initialize certain properties.
        loop {
            let c = Chunk::load(rdr)?;
            match c {
                Chunk::ActorName { name, visible } => {
                    println!("Actor {} visible {}", name, visible);
                    let child_id: NodeId = actor.tree.insert(
                        Node::new(ActorNode::Actor { name, visible }),
                        UnderNode(&current_actor)).unwrap();
                    last_actor = child_id.clone();
                },
                Chunk::ActorTransform(transform) => {
                    actor.tree.insert(
                        Node::new(ActorNode::Transform(transform)),
                        // Transform is unconditionally attached to the last loaded actor
                        UnderNode(&last_actor)).unwrap();
                },
                Chunk::MaterialRef(name) => {
                    actor.tree.insert(
                        Node::new(ActorNode::MaterialRef(name)),
                        UnderNode(&current_actor)).unwrap();
                },
                Chunk::MeshFileRef(name) => {
                    actor.tree.insert(
                        Node::new(ActorNode::MeshfileRef(name)),
                        UnderNode(&current_actor)).unwrap();
                },
                Chunk::ActorNodeDown() => {
                    current_actor = last_actor.clone();
                },
                Chunk::ActorNodeUp() => {
                    let node = actor.tree.get(&current_actor).unwrap();
                    if let Some(parent) = node.parent() {
                        current_actor = parent.clone();
                    }
                },
                Chunk::Null() => break,
                Chunk::FileHeader { file_type } => {
                    if file_type != support::ACTOR_FILE_TYPE {
                        panic!("Invalid model file type {}", file_type);
                    }
                },
                _ => unimplemented!(), // unexpected type here
            }
        }
        }

        Ok(actor)
    }

    pub fn load_from(fname: String) -> Result<Vec<Actor>, Error> {
        let file = File::open(fname)?;
        let mut file = BufReader::new(file);
        let mut models = Vec::<Actor>::new();
        loop {
            let m = Actor::load(&mut file);
            match m {
                Err(_) => break, // fixme: allow only Eof here
                Ok(m) => models.push(m),
            }
        }
        Ok(models)
    }
}
