__constant float PI = 3.14159265359f;
__constant float r_root2 = 0.577f;
__constant float max_reflect = 20.0f;
__constant float far_plane = 20.0f;
__constant float near_plane = 0.1f;
__constant float eps = 0.005f;

typedef struct Ray{
	float3 origin;
	float3 dir;
} Ray;

typedef struct Object{
	float radius;
	float type;
	float reflectance;
	float geometry_type;
	float3 position;
	float3 color;
	float3 emission;
} Object;

typedef struct Camera{
	float3 position;		
	float3 view;			
	float3 up;			
	float2 resolution;	
	float2 fov;		
	float apertureRadius;
	float focalDistance;
} Camera;

Ray createCamRay(const int x_coord, const int y_coord, const int width, const int height, __constant Camera* cam){

	/* create a local coordinate frame for the camera */
	float3 rendercamview = cam->view; rendercamview = fast_normalize(rendercamview);
	float3 rendercamup = cam->up; rendercamup = fast_normalize(rendercamup);
	float3 horizontalAxis = cross(rendercamview, rendercamup); horizontalAxis = fast_normalize(horizontalAxis);
	float3 verticalAxis = cross(horizontalAxis, rendercamview); verticalAxis = fast_normalize(verticalAxis);

	float3 middle = cam->position + rendercamview;
	float3 horizontal = horizontalAxis * tan(cam->fov.x * 0.5f * (PI / 180)); 
	float3 vertical  = verticalAxis * tan(cam->fov.y * -0.5f * (PI / 180)); 

	unsigned int x = x_coord;
	unsigned int y = y_coord;

	int pixelx = x_coord; 
	int pixely = height - y_coord - 1;

	float sx = (float)pixelx / (width - 1.0f);
	float sy = (float)pixely / (height - 1.0f);
	
	float3 pointOnPlaneOneUnitAwayFromEye = middle + (horizontal * ((2 * sx) - 1)) + (vertical * ((2 * sy) - 1));
	float3 pointOnImagePlane = cam->position + ((pointOnPlaneOneUnitAwayFromEye - cam->position) * cam->focalDistance); /* cam->focalDistance */

	float3 aperturePoint;

	aperturePoint = cam->position;

	float3 apertureToImagePlane = pointOnImagePlane - aperturePoint; apertureToImagePlane = fast_normalize(apertureToImagePlane); 

	Ray ray;
	ray.origin = aperturePoint;
	ray.dir = apertureToImagePlane; 

	return ray;
}

void set_bit(uint* a, uint i, uint val) {
	if(val)	{ *a |= 1 << i; return;}
	*a &= ~(1 << i);
}

int read_bit(uint* a, uint i) { return (*a & (1 << i)); }

void gen_obj_list(uint* a, __constant Object* objects, const int obj_count, const Ray* ray) {
	int i = 0; *a = 0;
	while(i < obj_count) {
		float dp = dot(fast_normalize(objects[i].position - ray->origin), ray->dir);
		if(dp > 0) {
			float ray_obj_angle = acos(dp);
			float length_to_center = fast_length(objects[i].position - ray->origin);
			if(( length_to_center * sin(ray_obj_angle)) <= objects[i].radius) {*a += (1 << i);}
		}
		i++;
	}
}

float sdf_sphere(float3 p, Object o) { return fast_distance(o.position, p) - o.radius; }

float sdf_box(float3 p, Object o, global float4* albedo) {
	float s = o.radius * r_root2;
	float3 v = fabs(p - o.position) - s;

	if(v.x < 0.01f && v.y < 0.01f && v.z < 0.01f) {	
		return 2.5f * eps;
		/*
		int vol_x, vol_y, vol_z;
		vol_x = (int) 102 * ((o.position.x - p.x + s) / (2 * s));
		vol_y = (int) 102 * ((o.position.y - p.y + s) / (2 * s));
		vol_z = (int) 100 * ((o.position.z - (p.z * 2.0f) + s) / (2 * s));
		float sample = albedo[vol_x + (vol_y * 1024) + ((vol_z % 10) * 102 + (vol_z / 10) * 104448)].y;
		if(sample > 0.7f) return 0;
		return 2.0f * eps * (1.0f - sample);
		*/
	}
	return max(max(v.x, v.y), v.z);
}

// Mandelbulb Fractal Distance Field Equation
/* 
float sdf_mandelbulb(float3 p, Object o) {
	float3 z = p;
	float dr = 1.0f;
	float r = 0.0f;
	float power = 8.0f;
	int i = 0;
	while(i < 5) {
		r = fast_length(z);
		if (r > 2.0f) break;
		
		float theta = acos(z.z / r);
		float phi = atan(z.y / z.x);
		dr = pow(r, power-1.0f) * power * dr + 1.0f;
		
		float zr = pow( r, power);
		theta = theta * power;
		phi = phi * power;
		
		z = zr * (float3)(sin(theta)*cos(phi), sin(phi)*sin(theta), cos(theta));
		z += p;
		i++;
	}
	return 0.5f * half_log(r) * r / dr;
}
*/
float smin(float a, float b, float c) {
	float res = clamp(0.5f + 0.5f*(b-a)/c, 0.0f, 1.0f);
	return mix(b, a, res) - c*res*(1.0f - res);
}

float get_sdf(float3 p, Object o, global float4* albedo) {
	float type = o.type;
	if(type == 1.0f) {
		float3 normal = fast_normalize(o.position - p);
		/*int tex_pos_x = (int) (fabs(p.x * 500));
		int tex_pos_z = (int) (fabs(p.z * 500));*/
		int tex_pos_x = 1 * 1024 * (0.5f + atan(normal.z / normal.x) / PI);
		int tex_pos_z = 1 * 1024 * (0.5f + asin(normal.y) / PI);
		tex_pos_x %= 1024; tex_pos_z %= 1024;
		/*if(p.x < 0) tex_pos_x = 1023 - tex_pos_x;
		if(p.z < 0) tex_pos_z = 1023 - tex_pos_z;*/
		float tex_height = 1.0f - albedo[tex_pos_x + (1024 * tex_pos_z)].w;
		return sdf_sphere(p, o) + (o.radius * 0.0001f) * tex_height;
	}
	/*if(type == 0.5f) return sdf_mandelbulb(p, o);*/
	if(type == 0.7f) return sdf_box(p, o, albedo);
	return sdf_sphere(p, o);
}

float map(float3 p, __constant Object* objects, const int obj_count, int* color_id, uint* a, global float4* albedo) {
	float sdf_min = far_plane;
	if(*a == 0) { *color_id = -1; return sdf_min; }
	float p_min;
	int i = 0;
	while(i < obj_count) {
		p_min = sdf_min;
		if (read_bit(a, i)) {
			if(objects[i].geometry_type) sdf_min = max(sdf_min, -get_sdf(p, objects[i], albedo));
			else sdf_min = min(sdf_min, get_sdf(p, objects[i], albedo));
		}				
		if(p_min != sdf_min) { *color_id = i; }
		i++;
	}
	return sdf_min;
}

float3 normal(float3 p, __constant Object* objects, const int obj_count, int* color_id, uint* a, global float4* albedo) {
	float3 norm_vector = fast_normalize((float3) (map(p + (float3)(eps, 0, 0), objects, obj_count, color_id, a, albedo) - map(p - (float3)(eps, 0, 0), objects, obj_count, color_id, a, albedo),
					map(p + (float3)(0, eps, 0), objects, obj_count, color_id, a, albedo) - map(p - (float3)(0, eps, 0), objects, obj_count, color_id, a, albedo),
					map(p + (float3)(0, 0, eps), objects, obj_count, color_id, a, albedo) - map(p - (float3)(0, 0, eps), objects, obj_count, color_id, a, albedo)));
	/*norm_vector * albedo[(int) p.x];*/
}

float shadow(float3 origin, float3 dir, __constant Object* objects, const int obj_count, int* color_id, uint* a, global float4* albedo) {
	Ray ray; ray.origin = origin; ray.dir = dir;
	/**a = 0-1;*/
	gen_obj_list(a, objects, obj_count, &ray);
	set_bit(a, *color_id, 0);
	if(objects[*color_id].type == 0.7f) set_bit(a, *color_id, 1);
	float res = 1.0f;
	if(*a == 0) return res;
	int i = 0;
	float step = 2.0f * eps;
	while(i < 32) {
		float sdf = map(origin + dir * step, objects, obj_count, color_id, a, albedo);
		if(objects[*color_id].type == 0.7f && fabs(sdf) <= 10.0f * eps) {
			res -= 0.1f;
			if(res < 0.0f) res = 0.0f;
		}
		if(fabs(sdf) < eps) return 0.0f;
		res = min(res, 4 * (sdf / step));
		step += sdf;
		if(step > far_plane) break;
		i++;
	}
	return res;
}

float ambient_occlusion(float3 p, float3 normal, __constant Object* objects, const int obj_count, int* color_id, uint* a, global float4* albedo) {
	Ray ray; ray.origin = p; ray.dir = normal;
	gen_obj_list(a, objects, obj_count, &ray);
	if(*a == 0) { return 1.0f;}
	set_bit(a, *color_id, 1);
	return 100.0f * min(map(p + (0.01f * normal), objects, obj_count, color_id, a, albedo), 0.01f);
}

float reflect(float3 origin, float3 dir, __constant Object* objects, const int obj_count, int* color_id, uint* a, global float4* albedo) {
	Ray ray; ray.origin = origin; ray.dir = dir;
	gen_obj_list(a, objects, obj_count, &ray);
	/*set_bit(a, *color_id, 0);*/
	/*if(objects[*color_id].type == 1.0f) set_bit(a, *color_id, 1);*/
	if(*a == 0) { *color_id = -1; return max_reflect; }
	
	float step = 2.0f * eps;
	int i = 0;
	while(i < 32) {
		float sdf = map(origin + dir * step, objects, obj_count, color_id, a, albedo);
		if(fabs(sdf) < eps) return step;
		step += sdf;
		if(step >= max_reflect) { *color_id = -1; return max_reflect; }
		i++;
	}
	*color_id = -1;
	return max_reflect;
}

float3 fog(float3 color, float distance) {
	float fogAmount = exp(-distance * 0.02f);
	float3 fogColor = (float3) (0.7f, 0.7f, 1.0f);
	return mix(fogColor, color, fogAmount);
}

float3 getColor(int color_id, __constant Object* objects, float3 cam_step, __global float4* albedo, float3 normal) {
	if(objects[color_id].type == 1.0f) {
		int tex_pos_x = (int) (fabs((cam_step.x) * 500));
		int tex_pos_z = (int) (fabs((cam_step.z) * 500));
		/*int tex_pos_x = 1 * 1024 * (0.5f + atan(normal.z / normal.x) / (PI));
		int tex_pos_z = 1 * 1024 * (0.5f - asin(normal.y) / (PI));*/
		tex_pos_x %= 1024; tex_pos_z %= 1024;
		/*if(cam_step.x < 0) tex_pos_x = 1023 - tex_pos_x;
		if(cam_step.z < 0) tex_pos_z = 1023 - tex_pos_z;*/
		return albedo[tex_pos_x + (1024 * tex_pos_z)].xyz;
	}
	
	if(objects[color_id].type == 0.7f) {
		float s = objects[color_id].radius * r_root2;
		int vol_x = (int) 102 * ((objects[color_id].position.x - cam_step.x + s) / (s * 2));
		int vol_y = (int) 102 * ((objects[color_id].position.y - cam_step.y + s) / (s * 2));
		int vol_z = (int) 100 * ((objects[color_id].position.z - cam_step.z + s) / (s * 2));
		/*int vol_z = (int) (25 * (cam_step.z + 2.0f)) % 101;*/
		vol_x %= 1024; vol_y %= 1024; vol_z %= 1024;
		return albedo[vol_x + (vol_y * 1024) + ((vol_z % 10) * 102 + (vol_z / 10) * 104448)].xyz;
	}
	
	/*
	if(objects[color_id].type == 0.5f) {
		return (float3) (fabs(cam_step.x - objects[color_id].position.x), fabs(cam_step.y - objects[color_id].position.y), fabs(cam_step.z - objects[color_id].position.z));
	}
	*/
	/* Coloring for fractal */
	return objects[color_id].color;
	
}

float3 raymarch(const Ray* camray, __constant Object* objects, const int obj_count, uint* a, __global float4* albedo) {
	Ray ray = *camray;

	float3 background_color = (float3) (0.7f, 0.7f, 1.0f);
	float3 ambient_color = (float3) (0.01f, 0.01f, 0.01f);
	float3 specular_color = (float3) (1.0f, 1.0f, 1.0f);
	float3 error_color = (float3) (1.0f, 0.0f, 1.0f);
	float3 volume_color = (float3) (0.0f, 0.0f, 0.0f);
	int color_id = 0, reflect_id = 0, normal_id = 0, shade_id = 0;

	float sdf = 0, step = near_plane;
	float3 cam_step, color = (float3)(0.0f, 0.0f, 0.0f);
	float i = 0;
	float max_steps = 256;
	while(i < max_steps) {	
		cam_step = ray.origin + ray.dir * step;
		
		
		sdf = map(cam_step, objects, obj_count, &color_id, a, albedo);
		if(objects[color_id].type == 0.7f && fabs(sdf) <= 10.0f * eps) {
			float s = objects[color_id].radius * r_root2;
			int vol_x = (int) 102 * ((objects[color_id].position.x - cam_step.x + s) / (2 * s));
			int vol_y = (int) 100 * ((objects[color_id].position.y - cam_step.y + s) / (2 * s));
			int vol_z = (int) 102 * ((objects[color_id].position.z - cam_step.z + s) / (2 * s));
			float3 vol_density = albedo[vol_x + (vol_z * 1024) + ((vol_y % 10) * 102 + (vol_y / 10) * 104448)].xyz;
			if(vol_density.x > 0.0f) volume_color = mix(2.0f * vol_density.x, volume_color, 0.1f);
		}
		if(step >= far_plane) return mix(volume_color, background_color, 0.5f);
		if(color_id == -1) return background_color;
		if(fabs(sdf) <= eps || i == max_steps - 1 || volume_color.x >= 1.0f) {
			shade_id = color_id;
			float3 n1 = normal(cam_step, objects, obj_count, &normal_id, a, albedo);
			float3 l1 = fast_normalize((float3) (0.2f, 0.3f, 0.8f));

			/* Get the color of the object */
			if(objects[color_id].type != 0.7f) color = mix(getColor(color_id, objects, cam_step, albedo, n1), color, 0.5f);	
			
			float3 r1 = fast_normalize(ray.dir - (n1 * 2 * dot(ray.dir, n1)));
			float rd = reflect(cam_step, r1, objects, obj_count, &reflect_id, a, albedo);
			float3 n2 = normal(cam_step + rd * r1, objects, obj_count, &reflect_id, a, albedo);

			float reflect_normal = (dot(l1, n2) + 1.0f) / 2.0f;
			float3 reflect_color = getColor(reflect_id, objects, cam_step + (rd * r1), albedo, n1);
			reflect_color = mix(reflect_color, ambient_color, 1.0f - ((reflect_normal) / 1.0f));
			reflect_color = mix(reflect_color, background_color, objects[reflect_id].reflectance);

			if(reflect_id == -1) color = mix(color, background_color, objects[color_id].reflectance);
			else color = mix(color, reflect_color, objects[color_id].reflectance);

			float shade = shadow(cam_step, l1, objects, obj_count, &shade_id, a, albedo);
			float normal = 0.5f * dot(l1, n1) + 0.5f;
			float AO = ambient_occlusion(cam_step, n1, objects, obj_count, &normal_id, a, albedo);

			color = mix(color, ambient_color, 1.0f - shade);
			color = mix(color, ambient_color, 1.0f - normal);
			/*color = mix(color, ambient_color, 1.0f - AO);*/
			/*color = mix(color, specular_color, objects[color_id].reflectance * pow(normal, 128.0f));*/

			color = fog(color, step);
			color = clamp(color, 0.0f, 1.0f);

			return mix(volume_color, color, 0.5f);
		}
		step += sdf;
		i++;
	}
	return error_color;
}			

__kernel void render_kernel(__global float4* albedo, __write_only image2d_t output, const int width, const int height, __constant Object* objects,
const int obj_count, const int sample, __constant const Camera* cam, const int framenumber) {

	unsigned int work_item_id = get_global_id(0);	/* the unique global id of the work item for the current pixel */
	unsigned int y_coord = ((sample * work_item_id) / width);
	unsigned int x_coord = ((sample * work_item_id) % width) + ((y_coord + (framenumber % sample)) % sample);
	
	Ray camray = createCamRay(x_coord, y_coord, width, height, cam);
	
	uint obj_mod = 0;
	gen_obj_list(&obj_mod, objects, obj_count, &camray);

	float4 finalcolor = (float4)(0.0f, 0.0f, 0.0f, 1.0f);
	finalcolor += (float4) (raymarch(&camray, objects, obj_count, &obj_mod, albedo), 1.0f);

	finalcolor = (float4)(clamp(finalcolor.x, 0.0f, 1.0f), clamp(finalcolor.y, 0.0f, 1.0f), clamp(finalcolor.z, 0.0f, 1.0f), 1.0f);
	finalcolor = pow(finalcolor, (float4)(1.0f/2.2f));

	int2 coord = (int2) (x_coord, y_coord);
	write_imagef(output, coord, finalcolor);
	/*output[x_coord + y_coord * width] = finalcolor;*/
}