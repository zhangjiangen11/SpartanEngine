/*
Copyright(c) 2016-2025 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES =================================
#include "pch.h"
#include "Terrain.h"
#include "Renderable.h"
#include "../Entity.h"
#include "../World.h"
#include "../../RHI/RHI_Texture.h"
#include "../../IO/FileStream.h"
#include "../../Resource/ResourceCache.h"
#include "../../Rendering/Mesh.h"
#include "../../Rendering/Material.h"
#include "../../Geometry/GeometryProcessing.h"
#include "../../Core/ThreadPool.h"
#include "../../Core/ProgressTracker.h"
//============================================

//= NAMESPACES ===============
using namespace std;
using namespace spartan::math;
//============================

namespace spartan
{
    namespace perlin
    {
        // permutation table (256 values, typically used in Perlin noise for randomness)
        static unsigned char p[512] = {
            151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
            8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
            35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
            134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
            55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
            18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,
            226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,
            17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,
            155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
            218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,
            249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,
            127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,
            61,156,180,
            // duplicate the array for wrapping (common practice in Perlin noise)
            151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
            8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
            35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
            134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
            55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
            18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,
            226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,
            17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,
            155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
            218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,
            249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,
            127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,
            61,156,180
        };
    
        // Fade function for smooth interpolation (6t^5 - 15t^4 + 10t^3)
        inline float fade(float t)
        {
            return t * t * t * (t * (t * 6 - 15) + 10);
        }
    
        // Linear interpolation
        inline float lerp(float a, float b, float t)
        {
            return a + t * (b - a);
        }
    
        // Gradient function: computes dot product between gradient vector and distance vector
        inline float grad(int hash, float x, float y)
        {
            int h = hash & 15;           // Take lower 4 bits of hash
            float u = h < 8 ? x : y;     // If h < 8, use x, else use y
            float v = h < 4 ? y : (h == 12 || h == 14 ? x : 0); // Select v based on hash
            return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v); // Dot product
        }
    
        // 2D Perlin noise function
        float noise(float x, float y)
        {
            // Find unit grid cell containing point
            int X = static_cast<int>(floor(x)) & 255;
            int Y = static_cast<int>(floor(y)) & 255;
    
            // Get relative coordinates within the cell
            x -= floor(x);
            y -= floor(y);
    
            // Compute fade curves for smooth interpolation
            float u = fade(x);
            float v = fade(y);
    
            // Hash coordinates of the 4 corners of the grid cell
            int aa = p[p[X] + Y];       // Bottom-left
            int ab = p[p[X] + Y + 1];   // Top-left
            int ba = p[p[X + 1] + Y];   // Bottom-right
            int bb = p[p[X + 1] + Y + 1]; // Top-right
    
            // Compute gradients and interpolate
            float g1 = grad(aa, x, y);           // Bottom-left gradient
            float g2 = grad(ba, x - 1, y);       // Bottom-right gradient
            float x1 = lerp(g1, g2, u);          // Interpolate along x (bottom edge)
            
            float g3 = grad(ab, x, y - 1);       // Top-left gradient
            float g4 = grad(bb, x - 1, y - 1);   // Top-right gradient
            float x2 = lerp(g3, g4, u);          // Interpolate along x (top edge)
    
            // Interpolate along y and return noise value in range [-1, 1]
            return lerp(x1, x2, v);
        }
    }

    namespace
    {
        const float sea_level               = 0.0f;      // the height at which the sea level is 0.0f; // this is an axiom of the engine
        const uint32_t scale                = 2;         // the scale factor to upscale the height map by
        const uint32_t smoothing_iterations = 0;         // the number of height map neighboring pixel averaging - useful if you are reading the height map with a scale of 1 (no bilinear interpolation)
        const uint32_t tile_count           = 8 * scale; // the number of tiles in each dimension to split the terrain into

        float compute_terrain_area_km2(const vector<RHI_Vertex_PosTexNorTan>& vertices)
        {
            if (vertices.empty())
                return 0.0f;
        
            // Initialize min and max values for x and z coordinates
            float min_x = numeric_limits<float>::max();
            float max_x = numeric_limits<float>::lowest();
            float min_z = numeric_limits<float>::max();
            float max_z = numeric_limits<float>::lowest();
        
            // iterate through all vertices to find the bounding box
            for (const auto& vertex : vertices)
            {
                float x = vertex.pos[0]; // x-coordinate
                float z = vertex.pos[2]; // z-coordinate
        
                // Update min and max values
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (z < min_z) min_z = z;
                if (z > max_z) max_z = z;
            }
        
            // calculate width (x extent) and depth (z extent) in meters
            float width = max_x - min_x;
            float depth = max_z - min_z;
        
            // compute area in square meters
            float area_m2 = width * depth;
        
            // convert to square kilometers (1 km� = 1,000,000 m�)
            float area_km2 = area_m2 / 1000000.0f;
        
            return area_km2;
        }

        void get_values_from_height_map(vector<float>& height_data_out, RHI_Texture* height_texture, const float min_y, const float max_y, const uint32_t scale)
        {
            vector<byte> height_data = height_texture->GetMip(0, 0).bytes;
            SP_ASSERT(height_data.size() > 0);
        
            // first pass: map the red channel values to heights in the range [min_y, max_y] (parallelized)
            {
                uint32_t bytes_per_pixel = (height_texture->GetChannelCount() * height_texture->GetBitsPerChannel()) / 8;
                uint32_t pixel_count = static_cast<uint32_t>(height_data.size() / bytes_per_pixel);
        
                // pre-allocate output vector
                height_data_out.resize(pixel_count);
        
                // parallel mapping of heights
                auto map_height = [&height_data_out, &height_data, bytes_per_pixel, min_y, max_y](uint32_t start_pixel, uint32_t end_pixel)
                {
                    for (uint32_t pixel = start_pixel; pixel < end_pixel; pixel++)
                    {
                        uint32_t byte_index = pixel * bytes_per_pixel;
                        float normalized_value = static_cast<float>(height_data[byte_index]) / 255.0f;
                        height_data_out[pixel] = min_y + normalized_value * (max_y - min_y);
                    }
                };
                ThreadPool::ParallelLoop(map_height, pixel_count);
            }
        
            // second pass: upscale the height map by bilinearly interpolating the height values (parallelized)
            if (scale > 1)
            {
                // get the dimensions of the original texture
                uint32_t width  = height_texture->GetWidth();
                uint32_t height = height_texture->GetHeight();
                uint32_t upscaled_width  = scale * width;
                uint32_t upscaled_height = scale * height;
        
                // create a new vector for the upscaled height map
                vector<float> upscaled_height_data(upscaled_width * upscaled_height);
        
                // helper function to safely access height values with clamping
                auto get_height = [&](uint32_t i, uint32_t j) {
                    i = min(i, width - 1);
                    j = min(j, height - 1);
                    return height_data_out[j * width + i];
                };
        
                // parallel upscaling of the height map
                auto upscale_pixel = [&upscaled_height_data, &get_height, width, height, upscaled_width, upscaled_height, scale](uint32_t start_index, uint32_t end_index)
                {
                    for (uint32_t index = start_index; index < end_index; index++)
                    {
                        uint32_t x = index % upscaled_width;
                        uint32_t y = index / upscaled_width;
        
                        // compute texture coordinates (u, v) in the range [0, 1]
                        float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(upscaled_width);
                        float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(upscaled_height);
        
                        // map to original texture pixel coordinates
                        float i_float = u * static_cast<float>(width);
                        float j_float = v * static_cast<float>(height);
        
                        // determine the four surrounding pixel indices
                        uint32_t i0 = static_cast<uint32_t>(floor(i_float));
                        uint32_t i1 = min(i0 + 1, width - 1);
                        uint32_t j0 = static_cast<uint32_t>(floor(j_float));
                        uint32_t j1 = min(j0 + 1, height - 1);
        
                        // compute interpolation weights
                        float frac_i = i_float - static_cast<float>(i0);
                        float frac_j = j_float - static_cast<float>(j0);
        
                        // get the four height values
                        float val00 = get_height(i0, j0); // top-left
                        float val10 = get_height(i1, j0); // top-right
                        float val01 = get_height(i0, j1); // bottom-left
                        float val11 = get_height(i1, j1); // bottom-right
        
                        // perform bilinear interpolation
                        float interpolated = (1.0f - frac_i) * (1.0f - frac_j) * val00 +
                                             frac_i * (1.0f - frac_j) * val10 +
                                             (1.0f - frac_i) * frac_j * val01 +
                                             frac_i * frac_j * val11;
        
                        // store the interpolated value
                        upscaled_height_data[y * upscaled_width + x] = interpolated;
                    }
                };
                ThreadPool::ParallelLoop(upscale_pixel, upscaled_width * upscaled_height);
        
                // replace the original height data with the upscaled data
                height_data_out = move(upscaled_height_data);
            }
        
            // third pass: smooth out the height map values, this will reduce hard terrain edges
            {
                const uint32_t width  = height_texture->GetWidth() * scale;
                const uint32_t height = height_texture->GetHeight()  * scale;
        
                for (uint32_t iteration = 0; iteration < smoothing_iterations; iteration++)
                {
                    vector<float> smoothed_height_data = height_data_out; // create a copy to store the smoothed data
        
                    for (uint32_t y = 0; y < height; y++)
                    {
                        for (uint32_t x = 0; x < width; x++)
                        {
                            float sum      = height_data_out[y * width + x];
                            uint32_t count = 1;
        
                            // iterate over neighboring pixels
                            for (int ny = -1; ny <= 1; ++ny)
                            {
                                for (int nx = -1; nx <= 1; ++nx)
                                {
                                    // skip self/center pixel
                                    if (nx == 0 && ny == 0)
                                        continue;
        
                                    uint32_t neighbor_x = x + nx;
                                    uint32_t neighbor_y = y + ny;
        
                                    // check boundaries
                                    if (neighbor_x >= 0 && neighbor_x < width && neighbor_y >= 0 && neighbor_y < height)
                                    {
                                        sum += height_data_out[neighbor_y * width + neighbor_x];
                                        count++;
                                    }
                                }
                            }
        
                            // average the sum
                            smoothed_height_data[y * width + x] = sum / static_cast<float>(count);
                        }
                    }
        
                    height_data_out = smoothed_height_data;
                }
            }
        }

        void add_perlin_noise(vector<float>& height_data, uint32_t width, uint32_t height, float frequency, float amplitude)
        {
            // parallel application of perlin noise to height data
            auto add_noise = [&height_data, width, height, frequency, amplitude](uint32_t start_index, uint32_t end_index)
            {
                for (uint32_t index = start_index; index < end_index; ++index)
                {
                    uint32_t i = index % width;
                    uint32_t j = index / width;
                    float x = static_cast<float>(i) - width * 0.5f;
                    float z = static_cast<float>(j) - height * 0.5f;
                    float noise_value = perlin::noise(x * frequency, z * frequency);
                    height_data[j * width + i] += noise_value * amplitude;
                }
            };
            ThreadPool::ParallelLoop(add_noise, width * height);
        }

        void generate_positions(vector<Vector3>& positions, const vector<float>& height_map, const uint32_t width, const uint32_t height)
        {
            SP_ASSERT_MSG(!height_map.empty(), "Height map is empty");
        
            // pre-allocate positions vector
            positions.resize(width * height);
        
            // parallel generation of positions
            auto generate_position_range = [&positions, &height_map, width, height](uint32_t start_index, uint32_t end_index)
            {
                for (uint32_t index = start_index; index < end_index; index++)
                {
                    // convert flat index to x,y coordinates
                    uint32_t x = index % width;
                    uint32_t y = index / width;
        
                    // center on the x and z axis
                    float centered_x = static_cast<float>(x) - width * 0.5f;
                    float centered_z = static_cast<float>(y) - height * 0.5f;
        
                    // get height from height_map
                    float height_value = height_map[index];
        
                    // assign position (no mutex needed since each index is unique)
                    positions[index] = Vector3(centered_x, height_value, centered_z);
                }
            };
        
            // calculate total number of positions and run parallel loop
            uint32_t total_positions = width * height;
            ThreadPool::ParallelLoop(generate_position_range, total_positions);
        }

        void generate_vertices_and_indices(vector<RHI_Vertex_PosTexNorTan>& terrain_vertices, vector<uint32_t>& terrain_indices, const vector<Vector3>& positions, const uint32_t width, const uint32_t height)
        {
            SP_ASSERT_MSG(!positions.empty(), "Positions are empty");

            Vector3 offset = Vector3::Zero;
            {
                // calculate offsets to center the terrain
                float offset_x   = -static_cast<float>(width) * 0.5f;
                float offset_z   = -static_cast<float>(height) * 0.5f;
                float min_height = FLT_MAX;

                // find the minimum height to align the lower part of the terrain at y = 0
                for (const Vector3& pos : positions)
                {
                    if (pos.y < min_height)
                    {
                        min_height = pos.y;
                    }
                }

                offset = Vector3(offset_x, -min_height, offset_z);
            }

            uint32_t index = 0;
            uint32_t k     = 0;
            for (uint32_t y = 0; y < height - 1; y++)
            {
                for (uint32_t x = 0; x < width - 1; x++)
                {
                    Vector3 position = positions[index] + offset;

                    float u = static_cast<float>(x) / static_cast<float>(width - 1);
                    float v = static_cast<float>(y) / static_cast<float>(height - 1);

                    const uint32_t index_bottom_left  = y * width + x;
                    const uint32_t index_bottom_right = y * width + x + 1;
                    const uint32_t index_top_left     = (y + 1) * width + x;
                    const uint32_t index_top_right    = (y + 1) * width + x + 1;

                    // bottom right of quad
                    index           = index_bottom_right;
                    terrain_indices[k]      = index;
                    terrain_vertices[index] = RHI_Vertex_PosTexNorTan(positions[index], Vector2(u + 1.0f / (width - 1), v + 1.0f / (height - 1)));

                    // bottom left of quad
                    index           = index_bottom_left;
                    terrain_indices[k + 1]  = index;
                    terrain_vertices[index] = RHI_Vertex_PosTexNorTan(positions[index], Vector2(u, v + 1.0f / (height - 1)));

                    // top left of quad
                    index           = index_top_left;
                    terrain_indices[k + 2]  = index;
                    terrain_vertices[index] = RHI_Vertex_PosTexNorTan(positions[index], Vector2(u, v));

                    // bottom right of quad
                    index           = index_bottom_right;
                    terrain_indices[k + 3]  = index;
                    terrain_vertices[index] = RHI_Vertex_PosTexNorTan(positions[index], Vector2(u + 1.0f / (width - 1), v + 1.0f / (height - 1)));

                    // top left of quad
                    index           = index_top_left;
                    terrain_indices[k + 4]  = index;
                    terrain_vertices[index] = RHI_Vertex_PosTexNorTan(positions[index], Vector2(u, v));

                    // top right of quad
                    index           = index_top_right;
                    terrain_indices[k + 5]  = index;
                    terrain_vertices[index] = RHI_Vertex_PosTexNorTan(positions[index], Vector2(u + 1.0f / (width - 1), v));

                    k += 6; // next quad
                }
            }
        }

        void generate_normals(const vector<uint32_t>& /*terrain_indices*/, vector<RHI_Vertex_PosTexNorTan>& terrain_vertices, uint32_t width, uint32_t height)
        {
            SP_ASSERT_MSG(!terrain_vertices.empty(), "Vertices are empty");
        
            auto compute_vertex_data = [&](uint32_t start, uint32_t end)
            {
                for (uint32_t index = start; index < end; index++)
                {
                    uint32_t i = index % width;
                    uint32_t j = index / width;
        
                    // compute normal using gradients
                    float h_left, h_right, h_bottom, h_top;
        
                    // x-direction gradient
                    if (i == 0)
                    {
                        h_left  = terrain_vertices[j * width + i].pos[1];
                        h_right = terrain_vertices[j * width + i + 1].pos[1];
                    }
                    else if (i == width - 1)
                    {
                        h_left  = terrain_vertices[j * width + i - 1].pos[1];
                        h_right = terrain_vertices[j * width + i].pos[1];
                    } else
                    {
                        h_left  = terrain_vertices[j * width + i - 1].pos[1];
                        h_right = terrain_vertices[j * width + i + 1].pos[1];
                    }
                    float dh_dx = (h_right - h_left) / (i == 0 || i == width - 1 ? 1.0f : 2.0f);
        
                    // z-direction gradient
                    if (j == 0)
                    {
                        h_bottom = terrain_vertices[j * width + i].pos[1];
                        h_top    = terrain_vertices[(j + 1) * width + i].pos[1];
                    }
                    else if (j == height - 1)
                    {
                        h_bottom = terrain_vertices[(j - 1) * width + i].pos[1];
                        h_top    = terrain_vertices[j * width + i].pos[1];
                    }
                    else
                    {
                        h_bottom = terrain_vertices[(j - 1) * width + i].pos[1];
                        h_top    = terrain_vertices[(j + 1) * width + i].pos[1];
                    }
                    float dh_dz = (h_top - h_bottom) / (j == 0 || j == height - 1 ? 1.0f : 2.0f);

                    // normal
                    Vector3 normal(-dh_dx, 1.0f, -dh_dz);
                    normal.Normalize();
                    terrain_vertices[index].nor[0] = normal.x;
                    terrain_vertices[index].nor[1] = normal.y;
                    terrain_vertices[index].nor[2] = normal.z;
        
                    // tangent
                    Vector3 tangent(1.0f, 0.0f, 0.0f);
                    float proj  = Vector3::Dot(normal, tangent);
                    tangent     -= normal * proj; // Orthogonalize to normal
                    tangent.Normalize();
                    terrain_vertices[index].tan[0] = tangent.x;
                    terrain_vertices[index].tan[1] = tangent.y;
                    terrain_vertices[index].tan[2] = tangent.z;
                }
            };
        
            ThreadPool::ParallelLoop(compute_vertex_data, static_cast<uint32_t>(terrain_vertices.size()));
        }

        float get_random_float(mt19937& gen, float x, float y)
        {
            uniform_real_distribution<float> distr(x, y); // define the distribution
            return distr(gen);
        }

       vector<Matrix> generate_transforms(const vector<RHI_Vertex_PosTexNorTan>& terrain_vertices, const vector<uint32_t>& terrain_indices, uint32_t transform_count, float max_slope_radians, bool rotate_to_match_surface_normal,float terrain_offset, float min_height)
       {
           // step 1: precompute acceptable triangles - this means within acceptable slope and height
            vector<uint32_t> acceptable_triangles;
            {
                acceptable_triangles.reserve(terrain_indices.size() / 3);

                for (uint32_t i = 0; i < terrain_indices.size(); i += 3)
                {
                    uint32_t idx0 = terrain_indices[i];
                    uint32_t idx1 = terrain_indices[i + 1];
                    uint32_t idx2 = terrain_indices[i + 2];
                
                    Vector3 v0(terrain_vertices[idx0].pos[0], terrain_vertices[idx0].pos[1], terrain_vertices[idx0].pos[2]);
                    Vector3 v1(terrain_vertices[idx1].pos[0], terrain_vertices[idx1].pos[1], terrain_vertices[idx1].pos[2]);
                    Vector3 v2(terrain_vertices[idx2].pos[0], terrain_vertices[idx2].pos[1], terrain_vertices[idx2].pos[2]);
                
                    Vector3 normal            = Vector3::Cross(v1 - v0, v2 - v0).Normalized();
                    float slope_radians       = acos(Vector3::Dot(normal, Vector3::Up));
                    bool is_acceptable_slope  = slope_radians <= max_slope_radians;
                    bool is_acceptable_height = v0.y >= min_height && v1.y >= min_height && v2.y >= min_height;
                
                    if (is_acceptable_slope && is_acceptable_height)
                    {
                        acceptable_triangles.push_back(i / 3); // store triangle index (divided by 3 since indices are per vertex)
                    }
                }
                
                if (acceptable_triangles.empty())
                    return {};
            }

            // step 2: prepare output vector and mutex
            vector<Matrix> transforms;
            {
                transforms.reserve(transform_count);
                mutex mtx;
                
                // step 3: parallel placement with local storage
                auto place_mesh = [&terrain_vertices, &terrain_indices, &acceptable_triangles, &transforms, &mtx, rotate_to_match_surface_normal, terrain_offset](uint32_t start_index, uint32_t end_index)
                {
                    // thread-local local resources and reservations (for maximum performance - no mutexes)
                    thread_local mt19937 generator(random_device{}());
                    uniform_int_distribution<> triangle_dist(0, static_cast<uint32_t>(acceptable_triangles.size() - 1));
                    uniform_real_distribution<float> barycentric_dist(0.0f, 1.0f);
                    uniform_real_distribution<float> angle_dist(0.0f, 360.0f);
                    vector<Matrix> local_transforms;
                    local_transforms.reserve(end_index - start_index);
                
                    for (uint32_t i = start_index; i < end_index; i++)
                    {
                        // select a random acceptable triangle
                        uint32_t tri_idx = acceptable_triangles[triangle_dist(generator)];
                        uint32_t index0 = terrain_indices[tri_idx * 3];
                        uint32_t index1 = terrain_indices[tri_idx * 3 + 1];
                        uint32_t index2 = terrain_indices[tri_idx * 3 + 2];
                
                        Vector3 v0(terrain_vertices[index0].pos[0], terrain_vertices[index0].pos[1], terrain_vertices[index0].pos[2]);
                        Vector3 v1(terrain_vertices[index1].pos[0], terrain_vertices[index1].pos[1], terrain_vertices[index1].pos[2]);
                        Vector3 v2(terrain_vertices[index2].pos[0], terrain_vertices[index2].pos[1], terrain_vertices[index2].pos[2]);
                
                        // generate barycentric coordinates
                        float u = barycentric_dist(generator);
                        float v = barycentric_dist(generator);
                        if (u + v > 1.0f)
                        {
                            u = 1.0f - u;
                            v = 1.0f - v;
                        }
                
                        // compute position with offset
                        Vector3 position = v0 + u * (v1 - v0) + v * (v2 - v0) + Vector3(0.0f, terrain_offset, 0.0f);
                
                        // compute rotation
                        Vector3 normal               = Vector3::Cross(v1 - v0, v2 - v0).Normalized();
                        Quaternion rotate_to_normal  = rotate_to_match_surface_normal ? Quaternion::FromToRotation(Vector3::Up, normal) : Quaternion::Identity;
                        Quaternion random_y_rotation = Quaternion::FromEulerAngles(0.0f, angle_dist(generator), 0.0f);
                        Quaternion rotation          = rotate_to_normal * random_y_rotation;
                
                        // create transform matrix (assuming Matrix constructor takes position, rotation, scale)
                        Matrix transform = Matrix::CreateScale(1.0f) *  Matrix::CreateRotation(rotation) * Matrix::CreateTranslation(position);
                        local_transforms.push_back(transform);
                    }
                
                    // merge local transforms into the shared vector
                    lock_guard<mutex> lock(mtx);
                    transforms.insert(transforms.end(), local_transforms.begin(), local_transforms.end());
                };
                
                // step 4: execute parallel loop
                ThreadPool::ParallelLoop(place_mesh, transform_count);
                
                return transforms;
            }
        }
    }

    Terrain::Terrain(Entity* entity) : Component(entity)
    {
        m_material = make_shared<Material>();
        m_material->SetObjectName("terrain");
    }

    Terrain::~Terrain()
    {
        m_height_texture = nullptr;
    }

    void Terrain::Serialize(FileStream* stream)
    {
        SP_LOG_WARNING("Not implemented");
    }

    void Terrain::Deserialize(FileStream* stream)
    {
        SP_LOG_WARNING("Not implemented");
    }

    uint32_t Terrain::GetWidth() const
    {
        return m_height_texture->GetWidth() * scale;
    }

    uint32_t Terrain::GetHeight() const
    {
        return m_height_texture->GetHeight() * scale;
    }

    void Terrain::GenerateTransforms(vector<Matrix>* transforms, const uint32_t count, const TerrainProp terrain_prop)
    {
        bool rotate_match_surface_normal = false; // don't rotate to match the surface normal
        float max_slope                  = 0.0f;  // don't allow slope
        float terrain_offset             = 0.0f;  // place exactly on the terrain
        float min_height                 = 0.0f;  // spawn at sea level= 0.0f; // spawn at sea level
    
        if (terrain_prop == TerrainProp::Tree)
        {
            max_slope                   = 30.0f * math::deg_to_rad;
            terrain_offset              = -2.0f; // push the tree slightly into the ground
            min_height                  = 6.0f;
        }
    
        if (terrain_prop == TerrainProp::Grass)
        {
            max_slope                   = 40.0f * math::deg_to_rad;
            rotate_match_surface_normal = true; // small plants tend to grow towards the sun but they can have some wonky angles
            min_height                  = 0.5f;
        }
    
        *transforms = generate_transforms(m_vertices, m_indices, count, max_slope, rotate_match_surface_normal, terrain_offset, min_height);
    }

    void Terrain::Generate()
    {
        // thread safety
        if (m_is_generating)
        {
            SP_LOG_WARNING("Terrain is already being generated, please wait...");
            return;
        }
    
        if (!m_height_texture)
        {
            SP_LOG_WARNING("You need to assign a height map before trying to generate a terrain");
            Clear();
            return;
        }
    
        m_is_generating = true;
    
        // start progress tracking
        uint32_t job_count = 8;
        ProgressTracker::GetProgress(ProgressType::Terrain).Start(job_count, "Generating terrain...");
    
        uint32_t width  = 0;
        uint32_t height = 0;
        vector<Vector3> positions;

        // note: the physics body reads the height map values, so any changes that need
        // to reflect on the collision shape, need to happen at the height value level

        // 1. process height map
        {
            ProgressTracker::GetProgress(ProgressType::Terrain).SetText("Process height map...");

            get_values_from_height_map(m_height_data, m_height_texture, m_min_y, m_max_y, scale);

            // deduce some stuff
            width            = GetWidth();
            height           = GetHeight();
            m_height_samples = width * height;
            m_vertex_count   = m_height_samples;
            m_index_count    = m_vertex_count * 6;
            m_triangle_count = m_index_count / 3;

            // allocate memory for the calculations that follow
            positions  = vector<Vector3>(m_height_samples);
            m_vertices = vector<RHI_Vertex_PosTexNorTan>(m_vertex_count);
            m_indices  = vector<uint32_t>(m_index_count);

            ProgressTracker::GetProgress(ProgressType::Terrain).JobDone();
        }
    
        // 2. add perlin noise
        {
            ProgressTracker::GetProgress(ProgressType::Terrain).SetText("Adding Perlin noise...");
            const float frequency = 0.1f;
            const float amplitude = 1.0f;
            add_perlin_noise(m_height_data, width, height, frequency, amplitude);
            ProgressTracker::GetProgress(ProgressType::Terrain).JobDone();
        }
    
        // 3. compute positions 
        {
            ProgressTracker::GetProgress(ProgressType::Terrain).SetText("Generating positions...");
            generate_positions(positions, m_height_data, width, height);
            ProgressTracker::GetProgress(ProgressType::Terrain).JobDone();
        }
    
        // 4. compute vertices and indices
        {
            ProgressTracker::GetProgress(ProgressType::Terrain).SetText("Generating vertices and indices...");
            generate_vertices_and_indices(m_vertices, m_indices, positions, width, height);
            ProgressTracker::GetProgress(ProgressType::Terrain).JobDone();
        }
    
        // 5. compute normals and tangents
        {
            ProgressTracker::GetProgress(ProgressType::Terrain).SetText("Generating normals...");
            generate_normals(m_indices, m_vertices, width, height);
            ProgressTracker::GetProgress(ProgressType::Terrain).JobDone();
        }
    
        // 6. optimize geometry 
        {
            ProgressTracker::GetProgress(ProgressType::Terrain).SetText("Optimizing geometry...");
            spartan::geometry_processing::optimize(m_vertices, m_indices);
            ProgressTracker::GetProgress(ProgressType::Terrain).JobDone();
        }
    
        // 7. split into tiles
        {
            ProgressTracker::GetProgress(ProgressType::Terrain).SetText("Splitting into tiles...");
            spartan::geometry_processing::split_surface_into_tiles(m_vertices, m_indices, tile_count, m_tile_vertices, m_tile_indices);
            ProgressTracker::GetProgress(ProgressType::Terrain).JobDone();
        }
    
        // 8. create a mesh for each tile
        {
            ProgressTracker::GetProgress(ProgressType::Terrain).SetText("Creating tile meshes");
            for (uint32_t tile_index = 0; tile_index < static_cast<uint32_t>(m_tile_vertices.size()); tile_index++)
            {
                UpdateMesh(tile_index);
            }
            ProgressTracker::GetProgress(ProgressType::Terrain).JobDone();
        }

        m_area_km2      = compute_terrain_area_km2(m_vertices);
        m_is_generating = false;
    }
    
    void Terrain::UpdateMesh(const uint32_t tile_index)
    {
        string name = "tile_" + to_string(tile_index);

        // create mesh if it doesn't exist
        if (m_tile_meshes.size() <= tile_index)
        {
            shared_ptr<Mesh>& mesh = m_tile_meshes.emplace_back(make_shared<Mesh>());
            mesh->SetObjectName(name);
            // don't optimize the terrain as tile seams will be visible
            mesh->SetFlag(static_cast<uint32_t>(MeshFlags::PostProcessOptimize), false);
        }

        // update with geometry
        shared_ptr<Mesh>& mesh = m_tile_meshes[tile_index];
        mesh->Clear();
        mesh->AddGeometry(m_tile_vertices[tile_index], m_tile_indices[tile_index], true);
        mesh->CreateGpuBuffers();

        // create a child entity, add a renderable, and this mesh tile to it
        {
            shared_ptr<Entity> entity = World::CreateEntity();
            entity->SetObjectName(name);
            entity->SetParent(World::GetEntityById(m_entity_ptr->GetObjectId()));

            if (Renderable* renderable = entity->AddComponent<Renderable>())
            {
                renderable->SetMesh(mesh.get());
                renderable->SetMaterial(m_material);
            }
        }
    }

    void Terrain::Clear()
    {
        m_vertices.clear();
        m_indices.clear();
        m_tile_meshes.clear();
        m_tile_vertices.clear();
        m_tile_indices.clear();

        for (auto& mesh : m_tile_meshes)
        {
            ResourceCache::Remove(mesh);
        }

        for (Entity* child : m_entity_ptr->GetChildren())
        {
            if (Renderable* renderable = child->AddComponent<Renderable>())
            {
                renderable->SetMesh(nullptr);
            }
        }
    }
}
