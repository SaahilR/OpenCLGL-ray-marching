
__kernel void render_kernel(__global float4* input, __write_only image2d_t resultTexture, const int width, const int height)
{	
	unsigned int global_work_id = get_global_id(0);
	unsigned int x_coord = global_work_id % width;
	unsigned int y_coord = global_work_id / width;
	int2 imgCoords = (int2)(x_coord, y_coord);

	float4 color = (float4) (0.0f, 0.0f, 1.0f, 1.0f);
	write_imagef(resultTexture, imgCoords, input[y_coord + (x_coord * 1024)]);
}