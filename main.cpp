#include <iostream>
#include <fstream>
#include <vector>
#include "gl_interop.h"
#include "linear_algebra.h"
#include "camera.h"
#include "player.h"
#include "geometry.h"
#include "user_interaction.h"

#define ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include "CL/cl2.hpp"

using namespace cl;

const int obj_count = 4;
// for variable quality within texture
// int sample_quality = 4;

// OpenCL objects
Device device;
CommandQueue queue;
Kernel kernel;
Context context;
Program program;
Buffer cl_output;
Buffer cl_obj;
Buffer cl_camera;
Buffer cl_accumbuffer;
ImageGL cl_texture;
BufferGL cl_albedo_texture;
BufferGL cl_normal_texture;
ImageGL cl_read_texture;
std::vector<Memory> cl_textures;
std::vector<Memory> cl_read_textures;
std::vector<Memory> cl_albedo_textures;
std::vector<Memory> cl_normal_textures;

cl_int err;
unsigned int framenumber = 0;

Camera* hostRendercam = NULL;
Object cpu_obj[obj_count];

void pickPlatform(Platform& platform, const std::vector<Platform>& platforms) {

	if (platforms.size() == 1) platform = platforms[0];
	else {
		unsigned int input = 0;
		std::cout << "\nChoose an OpenCL platform: ";
		std::cin >> input;

		// handle incorrect user input
		while (input < 1 || input > platforms.size()) {
			std::cin.clear(); //clear errors/bad flags on cin
			std::cin.ignore(std::cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
			std::cout << "No such option. Choose an OpenCL platform: ";
			std::cin >> input;
		}
		platform = platforms[input - 1];
	}
}

void pickDevice(Device& device, const std::vector<Device>& devices) {

	if (devices.size() == 1) device = devices[0];
	else {
		unsigned int input = 0;
		std::cout << "\nChoose an OpenCL device: ";
		std::cin >> input;

		// handle incorrect user input
		while (input < 1 || input > devices.size()) {
			std::cin.clear(); //clear errors/bad flags on cin
			std::cin.ignore(std::cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
			std::cout << "No such option. Choose an OpenCL device: ";
			std::cin >> input;
		}
		device = devices[input - 1];
	}
}

void printErrorLog(const Program& program, const Device& device) {

	// Get the error log and print to console
	std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
	std::cerr << "Build log:" << std::endl << buildlog << std::endl;

	// Print the error log to a file
	//FILE *log = fopen("errorlog.txt", "w");
	//fprintf(log, "%s\n", buildlog);
	std::cout << "Error log saved in 'errorlog.txt'" << std::endl;
	system("PAUSE");
	exit(1);
}

void initOpenCL()
{
	// Get all available OpenCL platforms (e.g. AMD OpenCL, Nvidia CUDA, Intel OpenCL)
	std::vector<Platform> platforms;
	Platform::get(&platforms);
	std::cout << "Available OpenCL platforms : " << std::endl << std::endl;
	for (unsigned int i = 0; i < platforms.size(); i++)
		std::cout << "\t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << std::endl;

	// Pick one platform
	Platform platform;
	pickPlatform(platform, platforms);
	std::cout << "\nUsing OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << std::endl;

	// Get available OpenCL devices on platform
	std::vector<Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

	std::cout << "Available OpenCL devices on this platform: " << std::endl << std::endl;
	for (unsigned int i = 0; i < devices.size(); i++) {
		std::cout << "\t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << std::endl;
		std::cout << "\t\tMax compute units: " << devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
		std::cout << "\t\tMax work group size: " << devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << std::endl << std::endl;
	}

	// Pick one device
	//Device device;
	pickDevice(device, devices);
	std::cout << "\nUsing OpenCL device: \t" << device.getInfo<CL_DEVICE_NAME>() << std::endl;
	std::cout << "\t\t\tMax compute units: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
	std::cout << "\t\t\tMax work group size: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << std::endl;

	// Create an OpenCL context on that device.
	// Windows specific OpenCL-OpenGL interop
	cl_context_properties properties[] =
	{
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
		0
	};

	context = Context(device, properties);

	// Create a command queue
	queue = CommandQueue(context, device);

	// Convert the OpenCL source code to a string
	std::string source;
	std::ifstream file("OpenCL/opencl_kernel.cl");
	if (!file) {
		std::cout << "\nNo OpenCL file found!" << std::endl << "Exiting..." << std::endl;
		system("PAUSE");
		exit(1);
	}
	while (!file.eof()) {
		char line[256];
		file.getline(line, 255);
		source += line;
	}

	const char* kernel_source = source.c_str();

	// Create an OpenCL program with source
	program = Program(context, kernel_source);

	// Build the program for the selected device 
	cl_int result = program.build({ device }); // "-cl-fast-relaxed-math"
	if (result) std::cout << "Error during compilation OpenCL code:\n (" << result << ")" << std::endl;
	if (result == CL_BUILD_PROGRAM_FAILURE) printErrorLog(program, device);
}

void initScene(Object* cpu_obj) {
	// floor
	cpu_obj[0].radius = 500.0f;
	cpu_obj[0].type = 0.0f;
	cpu_obj[0].reflectance = 0.0f;
	cpu_obj[0].geometry_type = 0.0f;
	cpu_obj[0].position = Vector3Df(0.0f, -500.5f, 0.0f);
	cpu_obj[0].color = Vector3Df(0.6f, 0.5f, 0.3f);
	cpu_obj[0].emission = Vector3Df(0.0f, 0.0f, 0.0f);
	
	cpu_obj[1].radius = 1.0f;
	cpu_obj[1].type = 0.7f;
	cpu_obj[1].reflectance = 0.0f;
	cpu_obj[1].geometry_type = 0.0f;
	cpu_obj[1].position = Vector3Df(0.10f, 1.5f, 0.4f);
	cpu_obj[1].color = Vector3Df(0.1f, 0.8f, 0.3f);
	cpu_obj[1].emission = Vector3Df(0.0f, 0.0f, 0.0f);
	
	cpu_obj[2].radius = 0.2f;
	cpu_obj[2].type = 0.0f;
	cpu_obj[2].reflectance = 0.1f;
	cpu_obj[2].geometry_type = 0.0f;
	cpu_obj[2].position = Vector3Df(0.0f, 0.0f, 0.0f);
	cpu_obj[2].color = Vector3Df(0.9f, 0.2f, 0.3f);
	cpu_obj[2].emission = Vector3Df(0.0f, 0.0f, 0.0f);

	// lightsource
	cpu_obj[3].radius = 0.2f;
	cpu_obj[3].type = 0.0f;
	cpu_obj[3].reflectance = 0.1f;
	cpu_obj[3].geometry_type = 0.0f;
	cpu_obj[3].position = Vector3Df(0.0f, 0.0f, 0.0f);
	cpu_obj[3].color = Vector3Df(0.0f, 0.9f, 0.7f);
	cpu_obj[3].emission = Vector3Df(0.0f, 0.0f, 0.0f);
	
}

// #define float3(x, y, z) {{x, y, z}}  // macro to replace ugly initializer braces

void initCLKernel() {
	
	// pick a rendermode
	unsigned int rendermode = 1;

	// Create a kernel (entry point in the OpenCL source program)
	kernel = Kernel(program, "render_kernel", &err);
	// specify OpenCL kernel arguments
	int i = 0;
	kernel.setArg(i++, cl_albedo_texture);
	//kernel.setArg(i++, cl_normal_texture);
	kernel.setArg(i++, cl_texture);
	kernel.setArg(i++, window_width);
	kernel.setArg(i++, window_height);
	kernel.setArg(i++, cl_obj);
	kernel.setArg(i++, obj_count);
	kernel.setArg(i++, interactiveCamera->getSampleQuality());
	kernel.setArg(i++, cl_camera);
	kernel.setArg(i++, framenumber);

	queue.finish();
}

void runKernel() {
	// NDRange used to define global and local work group size for the kernel
	NDRange global_work_size = (window_width * window_height) / interactiveCamera->getSampleQuality();
	NDRange local_work_size = kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device);;
	
	// Wait for OpenGL to finish before proceeding
	glFinish();
	// Enqueue texture to OpenCL
	queue.enqueueAcquireGLObjects(&cl_textures);
	queue.enqueueAcquireGLObjects(&cl_albedo_textures);
	//queue.enqueueAcquireGLObjects(&cl_normal_textures);
	//queue.enqueueAcquireGLObjects(&cl_read_textures);
	queue.finish();
	// Launch kernel with appropriate work group sizes
	queue.enqueueNDRangeKernel(kernel, NullRange, global_work_size, local_work_size); // local_work_size
	queue.finish();
	//Release the texture for OpenGL to use
	queue.enqueueReleaseGLObjects(&cl_textures);
	queue.enqueueReleaseGLObjects(&cl_albedo_textures);
	//queue.enqueueReleaseGLObjects(&cl_normal_textures);
	//queue.enqueueReleaseGLObjects(&cl_read_textures);
	queue.finish();
}

void render() {

	//cpu_obj[2].position = Vector3Df(cos(0.1f * framenumber), 0.1 * sin(0.4 * framenumber), 4.0f + sin(0.1f * framenumber));
	//cpu_obj[1].position = Vector3Df(0.3f, 2.0f * sin(0.03f * framenumber) - 0.4f, 0.1f);
	//cpu_obj[3].position = Vector3Df(0.3f, 0.1f * sin(0.02f * framenumber), 0.8f * sin(0.01f * framenumber));

	// Enqueue new information about objects to kernel
	queue.enqueueWriteBuffer(cl_obj, CL_TRUE, 0, obj_count * sizeof(Object), cpu_obj);
	// Increment framenumber (Change in objects tied to framerate not good)
	framenumber++;
	// Create a new camera on the cpu
	interactiveCamera->buildRenderCamera(hostRendercam);
	// Enqueue the camera information to the kernel
	queue.enqueueWriteBuffer(cl_camera, CL_TRUE, 0, sizeof(Camera), hostRendercam);
	queue.finish();

	// Update kernel arguments
	kernel.setArg(6, interactiveCamera->getSampleQuality());
	kernel.setArg(7, cl_camera);
	kernel.setArg(8, framenumber);

	runKernel();
	drawGL(interactiveCamera->getSampleQuality());
}

void cleanUp() {
	//	delete cpu_output;
}

// Initialize camera on the CPU
void initCamera() {
	delete interactiveCamera;
	interactiveCamera = new InteractiveCamera();

	interactiveCamera->setResolution(window_width, window_height);
	interactiveCamera->setFOVX(56.5f);
}
// Intialize the player 
void initPlayer() {
	delete interactiveCamera;
	interactiveCamera = new InteractiveCamera();

	interactiveCamera->setResolution(window_width, window_height);
	interactiveCamera->setFOVX(56.5f);
}

void main(int argc, char** argv) {		
	initGL(argc, argv);
	glFinish();
	std::cout << "Initialized OpenGL" << std::endl;
	initOpenCL();
	std::cout << "Initialized OpenCL" << std::endl;
	//createVertexBuffer(&vbo);
	createTexture(&window_texture);
	loadTexture("Texture/CTHeadS.png", &albedo_texture, &vbo_albedo);
	//loadTexture("Texture/rock1-normal.png", &normal_texture, &vbo_normal);

	std::cout << "Textures Created" << std::endl;
	Timer(0);
	
	// initialise scene
	initScene(cpu_obj);
	cl_obj = Buffer(context, CL_MEM_READ_ONLY, obj_count * sizeof(Object));
	queue.enqueueWriteBuffer(cl_obj, CL_TRUE, 0, obj_count * sizeof(Object), cpu_obj);

	// Initialise an interactive camera on the CPU side
	initCamera();
	// Create a CPU camera
	hostRendercam = new Camera();
	interactiveCamera->buildRenderCamera(hostRendercam);
	interactiveCamera->setSampleQuality(1);

	// Create buffer to hold information about camera
	cl_camera = Buffer(context, CL_MEM_READ_ONLY, sizeof(Camera));
	queue.enqueueWriteBuffer(cl_camera, CL_TRUE, 0, sizeof(Camera), hostRendercam);

	// create OpenCL buffer from OpenGL vertex buffer object
	cl_texture = ImageGL(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, window_texture, &err);
	//cl_texture = BufferGL(context, CL_MEM_WRITE_ONLY, vbo, &err);
	cl_textures.push_back(cl_texture);
	
	cl_albedo_texture = BufferGL(context, CL_MEM_READ_ONLY, vbo_albedo, &err);
	cl_albedo_textures.push_back(cl_albedo_texture);
	//cl_normal_texture = BufferGL(context, CL_MEM_READ_ONLY, vbo_normal, &err);
	//cl_normal_textures.push_back(cl_normal_texture);
	//cl_read_texture = ImageGL(context, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, input_texture, &err);
	//cl_read_textures.push_back(cl_read_texture);

	// intitialise the kernel
	initCLKernel();
	glutMainLoop();

	//cleanUp();
	system("Pause");
}