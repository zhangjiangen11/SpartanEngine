/*
Copyright(c) 2016-2023 Panos Karabelas

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

// this function is shared between depth_prepass.hlsl and g_buffer.hlsl, this is because the calculations have to be exactly the same

float4 compute_screen_space_position(Vertex_PosUvNorTan input, uint instance_id, matrix transform, matrix view_projection, inout float3 world_position)
{
    float4 position = input.position;
    position.w      = 1.0f;

    world_position = mul(position, transform).xyz;
    
    #if INSTANCED
    matrix instance = input.instance_transform;
    world_position  = mul(float4(world_position, 1.0f), instance).xyz;
    if (material_vertex_animate_wind())
    {
        world_position = vertex_simulation::wind::apply(instance_id, world_position, float3(instance._41, instance._42, instance._43), buffer_frame.time);
    }
    #endif

    if (material_vertex_animate_water())
    {
        world_position = vertex_simulation::water_wave::apply(world_position, buffer_frame.time);
    }

    return mul(float4(world_position, 1.0f), view_projection);
}

float4 compute_screen_space_position(Vertex_PosUvNorTan input, uint instance_id, matrix transform, matrix view_projection)
{
    float3 position_world_dummy = 0.0f;
    return compute_screen_space_position(input, instance_id, transform, view_projection, position_world_dummy);
}