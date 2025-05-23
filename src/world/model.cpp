#define TINYOBJLOADER_IMPLEMENTATION

#include "model.h"

#include "utils/error_handler.h"

#include <linalg.h>
#include <iostream>


using namespace linalg::aliases;
using namespace cg::world;

cg::world::model::model() {}

cg::world::model::~model() {}

void cg::world::model::load_obj(const std::filesystem::path& model_path)
{
	
	tinyobj::ObjReaderConfig readerConfig;
	readerConfig.mtl_search_path = model_path.parent_path().string();
	readerConfig.triangulate = true;

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(model_path.string(), readerConfig))
	{
		if (!reader.Error().empty())
		{
			THROW_ERROR(reader.Error());
		}
	}

	auto& shapes = reader.GetShapes();
	auto& attrib = reader.GetAttrib();
	auto& materials = reader.GetMaterials();

	allocate_buffers(shapes);
	
	fill_buffers(shapes, attrib, materials, model_path.parent_path());
}

void model::allocate_buffers(const std::vector<tinyobj::shape_t>& shapes)
{
	for (const auto& shape: shapes)
	{
		size_t index_offset = 0;
		unsigned int vertex_buffer_size = 0;
		unsigned int index_buffer_size = 0;
		std::map<std::tuple<int, int, int>, unsigned int> index_map;
		const auto& mesh = shape.mesh;
		for (const auto& fv : mesh.num_face_vertices)
		{
			for (size_t v = 0; v < fv; v++)
			{
				tinyobj::index_t idx = mesh.indices[index_offset + v];
				auto idx_tuple = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);
				if (index_map.count(idx_tuple) == 0)
				{
					index_map[idx_tuple] = vertex_buffer_size;
					vertex_buffer_size++;
			
				}
				index_buffer_size++;
			}
			index_offset += fv;
		}
		vertex_buffers.push_back(
			std::make_shared<cg::resource<cg::vertex>>(vertex_buffer_size)
		);
		index_buffers.push_back(
			std::make_shared<cg::resource<unsigned int>>(index_buffer_size)
		);
	}
	textures.resize(shapes.size());
}

float3 cg::world::model::compute_normal(const tinyobj::attrib_t& attrib, const tinyobj::mesh_t& mesh, size_t index_offset)
{
	auto a_id = mesh.indices[index_offset];
	auto b_id = mesh.indices[index_offset + 1];
	auto c_id = mesh.indices[index_offset + 2];

	float3 a{
		attrib.vertices[3 * a_id.vertex_index],
		attrib.vertices[3 * a_id.vertex_index + 1],
		attrib.vertices[3 * a_id.vertex_index + 2],
	};

	float3 b{
		attrib.vertices[3 * b_id.vertex_index],
		attrib.vertices[3 * b_id.vertex_index + 1],
		attrib.vertices[3 * b_id.vertex_index + 2],
	};

	float3 c{
		attrib.vertices[3 * c_id.vertex_index],
		attrib.vertices[3 * c_id.vertex_index + 1],
		attrib.vertices[3 * c_id.vertex_index + 2],
	};

	return normalize(cross(b -a, c - a));
}

void model::fill_vertex_data(cg::vertex& vertex, const tinyobj::attrib_t& attrib, const tinyobj::index_t idx, const float3 computed_normal, const tinyobj::material_t material)
{
	vertex.position = float3{
		attrib.vertices[3 * idx.vertex_index],
		attrib.vertices[3 * idx.vertex_index + 1],
		attrib.vertices[3 * idx.vertex_index + 2],
	};

	if (idx.normal_index < 0)
	{
		vertex.normal = computed_normal;
	}
	else
	{
		vertex.normal = float3{
			attrib.normals[3 * idx.normal_index],
			attrib.normals[3 * idx.normal_index + 1],
			attrib.normals[3 * idx.normal_index + 2],
		};
	}

	if (idx.texcoord_index < 0)
	{
		vertex.texture = float2{0, 0};
	}
	else
	{
		vertex.texture = float2{
			attrib.texcoords[3 * idx.texcoord_index],
			attrib.texcoords[3 * idx.texcoord_index + 1],
		};
	}

	vertex.ambient = float3{
		material.ambient[0],
		material.ambient[1],
		material.ambient[2],
	};

	vertex.diffuse = float3{
		material.diffuse[0],
		material.diffuse[1],
		material.diffuse[2],
	};

	vertex.emissive = float3{
		material.emission[0],
		material.emission[1],
		material.emission[2],
	};


}

void model::fill_buffers(const std::vector<tinyobj::shape_t>& shapes, const tinyobj::attrib_t& attrib, const std::vector<tinyobj::material_t>& materials, const std::filesystem::path& base_folder)
{
	for (size_t s = 0; s < shapes.size(); s++)
	{
		size_t index_offset = 0;
		unsigned int vertex_buffer_id = 0;
		unsigned int index_buffer_id = 0;
		auto vertex_buffer = vertex_buffers[s];
		auto index_buffer = index_buffers[s];
		std::map<std::tuple<int, int, int>, unsigned int> index_map;
		const auto& mesh = shapes[s].mesh;
		for (size_t f = 0; f < mesh.num_face_vertices.size(); f++)
		{
			int fv = mesh.num_face_vertices[f];
			float3 normal;
			if (mesh.indices[index_offset].normal_index < 0)
			{
				normal = compute_normal(attrib, mesh, index_offset);
			}
			for (size_t v = 0; v < fv; v++)
			{
				tinyobj::index_t idx = mesh.indices[index_offset + v];
				auto idx_tuple = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);
				if (index_map.count(idx_tuple) == 0)
				{
					cg:vertex& vertex = vertex_buffer->item(vertex_buffer_id);
					const auto& material = materials[mesh.material_ids[f]];

					fill_vertex_data(vertex, attrib, idx, normal, material);
					index_map[idx_tuple] = vertex_buffer_id;
					vertex_buffer_id++;
			
				}
				index_buffer->item(index_buffer_id) = index_map[idx_tuple];
				index_buffer_id++;
			}
			index_offset += fv;

		}
		if (!materials[mesh.material_ids[0]].diffuse_texname.empty())
		{
			textures[s] = base_folder / materials[mesh.material_ids[0]].diffuse_texname;
		}
	}
	textures.resize(shapes.size());
}


const std::vector<std::shared_ptr<cg::resource<cg::vertex>>>&
cg::world::model::get_vertex_buffers() const
{
	return vertex_buffers;
}

const std::vector<std::shared_ptr<cg::resource<unsigned int>>>&
cg::world::model::get_index_buffers() const
{
	return index_buffers;
}

const std::vector<std::filesystem::path>& cg::world::model::get_per_shape_texture_files() const
{
	return textures;
}


const float4x4 cg::world::model::get_world_matrix() const
{
	return float4x4{
			{1, 0, 0, 0},
			{0, 1, 0, 0},
			{0, 0, 1, 0},
			{0, 0, 0, 1}};
}
