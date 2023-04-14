#include <OpenGL/gl.h>
#include <GLUT/glut.h>

#include <math.h>
#include <printf.h>
#include <sys/time.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "shaders.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "cJSON/cJSON.h"
#include "base64/base64.h"


// Time since the last frame in seconds.
double time_delta;
double last_frame_time;

typedef struct {
    float x;
    float y;
    float z;
} vec3_t;

typedef struct {
    vec3_t position;

    GLfloat* vertices;
    GLuint* indices;
    GLfloat* texture_uvs; // TODO: Probably interleave with vertices.
    GLfloat* colors; // TODO: Remove.

    GLuint texture_id;

    int num_vertices;
    int num_indices;
} object_t;

typedef struct {
    int item_size;

    void* buffer;
    int free_offset;
} allocator_t;

allocator_t new_allocator(int item_size, int max_num_items) {
    allocator_t allocator = {
        .item_size   = item_size,
        .buffer      = aligned_alloc(512, max_num_items * item_size),
        .free_offset = 0
    };
    return allocator;
}

void* acquire_memory(allocator_t* allocator, int num_items) {
    void* acquired_pointer = allocator->buffer + allocator->free_offset * allocator->item_size;
    allocator->free_offset += num_items;
    return acquired_pointer;
}

allocator_t vertex_allocator;
allocator_t index_allocator;
allocator_t texture_uv_allocator;

double get_current_time() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (double)tp.tv_sec * 1000.0 + (double)tp.tv_nsec / 1000000.0;
}

void update_time_delta() {
    double current_time = get_current_time();
    time_delta = (current_time - last_frame_time) / 1000;
    last_frame_time = current_time;
}

void print_float_buffer(GLfloat* buffer, int num_items, int items_per_line) {
    int is_first = 1;
    for(int i = 0; i < num_items; i++) {
        if(!is_first) {
            printf(", ");
        }
        if(items_per_line != 0 && i % items_per_line == 0) {
            printf("\n%d: ", i / items_per_line);
        }
        printf("%.2f", buffer[i]);
        is_first = 0;
    }
    printf("\n");
}

void print_int_buffer(GLuint* buffer, int num_items) {
    int is_first = 1;
    for(int i = 0; i < num_items; i++) {
        if(!is_first) {
            printf(", ");
        }
        printf("%d", buffer[i]);
        is_first = 0;
    }
    printf("\n");
}

void get_texture_data(int texture_id) {
// bind the texture object to the 2D texture target
    glBindTexture(GL_TEXTURE_2D, texture_id);

// retrieve the texture image data
    GLint level = 0; // mipmap level (0 for base level)
    GLenum format = GL_RGBA; // format of the pixel data
    GLenum type = GL_FLOAT; // data type of the pixel data
    GLsizei width = 0; // width of the texture image (will be retrieved)
    GLsizei height = 0; // height of the texture image (will be retrieved)
    GLint imageSize = 0; // size of the image data (will be retrieved)
    void* pixels = NULL; // pointer to the image data (will be retrieved)

// get the width and height of the texture image
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &height);

// allocate memory for the image data
    imageSize = height * width * 4;
    pixels = malloc(imageSize * sizeof(float));

// retrieve the texture image data
    glGetTexImage(GL_TEXTURE_2D, level, format, type, pixels);
    print_float_buffer(pixels, imageSize, 0);

// free the image data memory
    free(pixels);
}


object_t create_pyramid(float base_width, float height, GLuint texture_id) {
    object_t triangle = {
        .position     = { .x = 0, .y = 0, .z = 0 },
        .num_vertices = 4,
        .num_indices  = 4,
        .texture_id   = texture_id,
    };
    GLuint v_start = vertex_allocator.free_offset;

    triangle.vertices = acquire_memory(&vertex_allocator, triangle.num_vertices);
    triangle.indices  = acquire_memory(&index_allocator, triangle.num_indices);
    triangle.texture_uvs   = acquire_memory(&texture_uv_allocator, triangle.num_vertices);

    /**
     * The first 3 values of each vector define the x, y, and z coordinate.
     * The 4th value is the homogenous coordinate, or w coordinate, and is included
     * so that we can do more types of matrix transforms(translation, etc). Its useful
     * to have a constant value that we can multiply by.
     */
    GLfloat template_vertices[] = {
        0.0f, 0.55f, 0.0f, 1.0f,     // Apex.
        0.43f, -0.4f, 0.24f, 1.0f,    // Front Right.
        -0.43f, -0.4f, 0.24f, 1.0f,   // Front Left.
        0.0f, -0.4f, -0.5f, 1.0f,   // Back.
    };
    memcpy(triangle.vertices, template_vertices, sizeof(template_vertices));

    GLuint template_indices[] = {
        v_start + 0, v_start + 2, v_start + 1,
        v_start + 0, v_start + 3, v_start + 2,
        v_start + 0, v_start + 1, v_start + 3,
        v_start + 1, v_start + 2, v_start + 3,
    };
    memcpy(triangle.indices, template_indices, sizeof(template_indices));

    GLfloat texture_uvs[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
    };
    memcpy(triangle.texture_uvs, texture_uvs, sizeof(texture_uvs));

    return triangle;
}

object_t create_cube(float side_width, GLuint texture_id) {
    object_t cube = {
            .position     = { .x = 0, .y = 0, .z = 0 },
            .num_vertices = 8,
            .num_indices  = 12,
            .texture_id   = texture_id,
    };
    GLuint v_start = vertex_allocator.free_offset;

    cube.vertices = acquire_memory(&vertex_allocator, cube.num_vertices);
    cube.indices  = acquire_memory(&index_allocator, cube.num_indices);
    cube.texture_uvs   = acquire_memory(&texture_uv_allocator, cube.num_vertices);

    /**
     * The first 3 values of each vector define the x, y, and z coordinate.
     * The 4th value is the homogenous coordinate, or w coordinate, and is included
     * so that we can do more types of matrix transforms(translation, etc). Its useful
     * to have a constant value that we can multiply by.
     */
    float len = side_width / 2;
    GLfloat template_vertices[] = {
            -len,  -len, -len, 1.0f,
            len,   -len, -len, 1.0f,
            len,   -len, len, 1.0f,
            -len, -len, len, 1.0f,

            -len, len, -len, 1.0f,
            len,  len, -len, 1.0f,
            len,  len, len,  1.0f,
            -len, len, len,  1.0f,
    };
    memcpy(cube.vertices, template_vertices, sizeof(template_vertices));

    GLuint template_indices[] = {
            v_start + 0, v_start + 1, v_start + 2,
            v_start + 0, v_start + 2, v_start + 3,

            v_start + 1, v_start + 0, v_start + 4,
            v_start + 1, v_start + 4, v_start + 5,

            v_start + 2, v_start + 1, v_start + 5,
            v_start + 2, v_start + 5, v_start + 6,

            v_start + 3, v_start + 2, v_start + 7,
            v_start + 2, v_start + 6, v_start + 7,

            v_start + 3, v_start + 7, v_start + 4,
            v_start + 3, v_start + 4, v_start + 0,

            v_start + 4, v_start + 6, v_start + 5,
            v_start + 4, v_start + 7, v_start + 6,
    };
    memcpy(cube.indices, template_indices, sizeof(template_indices));

    GLfloat template_uvs[] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f,

            1.0f, 0.0f,
            0.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
    };
    memcpy(cube.texture_uvs, template_uvs, sizeof(template_uvs));

    return cube;
}

object_t create_quad(float width, float height, GLuint texture_id) {
    object_t quad = {
            .position     = { .x = 0, .y = 0, .z = 0 },
            .num_vertices = 4,
            .num_indices  = 2,
            .texture_id   = texture_id,
    };
    GLuint v_start = vertex_allocator.free_offset;

    quad.vertices    = acquire_memory(&vertex_allocator, quad.num_vertices);
    quad.indices     = acquire_memory(&index_allocator, quad.num_indices);
    quad.texture_uvs = acquire_memory(&index_allocator, quad.num_vertices);

    GLfloat template_vertices[] = {
        -width / 2,  -height / 2, 0.0f, 1.0f,
        width / 2,   -height / 2, 0.0f, 1.0f,
        -width / 2,  height / 2, 0.0f, 1.0f,
        width / 2,   height / 2, 0.0f, 1.0f,
    };
    memcpy(quad.vertices, template_vertices, sizeof(template_vertices));

    GLuint template_indices[] = {
        0, 2, 1,
        1, 2, 3,
    };
    memcpy(quad.indices, template_indices, sizeof(template_indices));

    GLfloat texture_uvs[] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f,
    };
    memcpy(quad.texture_uvs, texture_uvs, sizeof(texture_uvs));

    return quad;
}

void apply_matrix_transform(GLfloat* vertex_pointer, int num_vertices, float matrix[4][4]) {
    for(int vertex_idx = 0; vertex_idx < num_vertices; vertex_idx++) {
        int idx = vertex_idx * 4;

        float result[4] = { 0, 0, 0, 0 };

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result[i] += matrix[i][j] * vertex_pointer[idx + j];
            }
        }
        for (int i = 0; i < 4; i++) {
            vertex_pointer[idx + i] = result[i];
        }
    }
}

void get_y_rotation_matrix(float output[4][4], float theta) {
    output[0][0] = cos(theta);  output[0][1] = 0; output[0][2] = sin(theta); output[0][3] = 0;
    output[1][0] = 0;           output[1][1] = 1; output[1][2] = 0;          output[1][3] = 0;
    output[2][0] = -sin(theta); output[2][1] = 0; output[2][2] = cos(theta); output[2][3] = 0;
    // NOTE: We need to set w to 1 here to ensure we don't zero it for future operations.
    output[3][0] = 0;           output[3][1] = 0; output[3][2] = 0;          output[3][3] = 1;
}

void get_x_rotation_matrix(float output[4][4], float theta) {
    output[0][0] = 1; output[0][1] = 0;          output[0][2] = 0;           output[0][3] = 0;
    output[1][0] = 0; output[1][1] = cos(theta); output[1][2] = -sin(theta); output[1][3] = 0;
    output[2][0] = 0; output[2][1] = sin(theta); output[2][2] = cos(theta);  output[2][3] = 0;
    output[3][0] = 0; output[3][1] = 0;          output[3][2] = 0;           output[3][3] = 1;
}

void get_translate_matrix(float output[4][4], float x_distance, float y_distance, float z_distance) {
    output[0][0] = 1; output[0][1] = 0; output[0][2] = 0; output[0][3] = x_distance;
    output[1][0] = 0; output[1][1] = 1; output[1][2] = 0; output[1][3] = y_distance;
    output[2][0] = 0; output[2][1] = 0; output[2][2] = 1; output[2][3] = z_distance;
    output[3][0] = 0; output[3][1] = 0; output[3][2] = 0; output[3][3] = 1;
}

vec3_t add_vectors(vec3_t vec1, vec3_t vec2) {
    vec3_t new_vec = {
        .x = vec1.x + vec2.x,
        .y = vec1.y + vec2.y,
        .z = vec1.z + vec2.z,
    };
    return new_vec;
}

void translate_object(object_t* shape, vec3_t distance) {
    float translation_matrix[4][4];
    get_translate_matrix(translation_matrix, distance.x, distance.y, distance.z);
    apply_matrix_transform(shape->vertices, shape->num_vertices, translation_matrix);

    shape->position = add_vectors(shape->position, distance);
}

void init_textures(GLuint* out_texture_id) {
    // /Users/jack/Downloads/Free 39 Portraits Pixel Art/2 Portraits with back

    // TODO: Use our own memory instead of STBI stuff.
    // TODO: if we load into floats, or pass ints to opengl, we can directly use the loaded buffer to provide the texture.
    int image_width, image_height, channels;
    char* image_path = "/Users/jack/Downloads/Free 39 Portraits Pixel Art/2 Portraits with back/Icons_05.png";
    unsigned char* image_data = stbi_load(image_path, &image_width, &image_height, &channels, STBI_rgb);
    if(image_data == NULL) {
        const char* error = stbi_failure_reason();
        printf("Failed to load image: %s\n", error);
        exit(-1);
    }

    printf("Loaded %dx%d image with %d channels.\n", image_width, image_height, channels);
    GLfloat* texture_data = (GLfloat*)malloc(image_width * image_height * 4 * sizeof(GLfloat));
    for (int pixel_idx = 0; pixel_idx < image_width * image_height; pixel_idx += 1) {
        int texture_offset = pixel_idx * 4;
        int image_offset = pixel_idx * 3;
        texture_data[texture_offset+0] = image_data[image_offset + 0] / 255.0f;
        texture_data[texture_offset+1] = image_data[image_offset + 1] / 255.0f;
        texture_data[texture_offset+2] = image_data[image_offset + 2] / 255.0f;
        texture_data[texture_offset+3] = 1.0f;
//        printf(
//                "%d, %d, %d\n",
//                image_data[image_offset+0],
//                image_data[image_offset+1],
//                image_data[image_offset+2]
//        );
//        printf(
//                "%.3f, %.3f, %.3f, %.3f\n",
//                texture_data[texture_offset+0],
//                texture_data[texture_offset+1],
//                texture_data[texture_offset+2],
//                texture_data[texture_offset+3]
//        );
    }

    stbi_image_free(image_data);

    glGenTextures(1, out_texture_id);
    glBindTexture(GL_TEXTURE_2D, *out_texture_id);

    // Set wrapping properties(clamp just uses the edge pixel if we exceed the edge).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Set the filtering properties for sampling.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height,
            0, GL_RGBA, GL_FLOAT, texture_data
    );

    // Unbind the texture.
    glBindTexture(GL_TEXTURE_2D, 0);

    free(texture_data);
}

void rotate_object(object_t* shape, float rotation_matrix[4][4]) {
    /**
     * All rotations happen around the origin so we need to translate back to the origin before rotating,
     * and translate back to our position after rotating.
     * TODO: Support multiple rotations at the origin without multiple translations in between. Oh actually might not
     * be necessary if rotation matrices can be combined.
     */
    vec3_t position = shape->position;

    float translation_matrix[4][4];
    get_translate_matrix(translation_matrix, -position.x, -position.y, -position.z);
    apply_matrix_transform(shape->vertices, shape->num_vertices, translation_matrix);

    apply_matrix_transform(shape->vertices, shape->num_vertices, rotation_matrix);

    get_translate_matrix(translation_matrix, position.x, position.y, position.z);
    apply_matrix_transform(shape->vertices, shape->num_vertices, translation_matrix);
}


GLuint shaderProgram;
GLuint vertexShader;
GLuint fragmentShader;

GLuint technicolor_texture;

object_t pyramid;
object_t cube;
object_t quad;
object_t model;

GLuint gl_vertex_array_object;
GLuint gl_vertex_buffer;
GLuint gl_texture_uv_buffer;

double total_time = 0;

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    update_time_delta();
    total_time += time_delta;

    // TODO: Its probably better to apply all transforms to each vertex as we iterate instead of iterating multiple times.
    float transform_matrix[4][4];

//    get_x_rotation_matrix(transform_matrix, 0.63f * time_delta);
//    rotate_object(&cube, transform_matrix);
    get_y_rotation_matrix(transform_matrix, 0.5 * time_delta);
    rotate_object(&model, transform_matrix);

//    vec3_t distance = {.x = 0, .y = 0, .z = -0.1f * time_delta};
//    translate_object(&model, distance);
//    get_x_rotation_matrix(transform_matrix, -0.5f * time_delta);
//    rotate_object(&model, transform_matrix);
//    get_y_rotation_matrix(transform_matrix, -0.8f * time_delta);
//    rotate_object(&pyramid, transform_matrix);

//    printf("============\n\n");
//    print_float_buffer(model.vertices, model.num_vertices * 3, 4);
//    exit(-1);

    // This code draws the shapes with a texture.

    // Load the shapes texture.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, model.texture_id);

    // Tell open GL to update the vertex and texture buffers with the new data.
    // TODO: We only need to update vertices that have moved since last frame.
    glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model.num_vertices * 4 * sizeof(GLfloat), model.vertices);

    // TODO: I think its pretty unlikely for texture UVs to change for an existing shape, this probably doesn't need
    // to happen each frame.
    glBindBuffer(GL_ARRAY_BUFFER, gl_texture_uv_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model.num_vertices * 2 * sizeof(GLfloat), model.texture_uvs);

    // Set the texture sampler for the shader. Because we're using GL_TEXTURE0 we set this to 0.
    GLint textureLocation = glGetUniformLocation(shaderProgram, "textureSampler");
    glUniform1i(textureLocation, 0);  // Set the value of the uniform variable to 0 (texture unit 0)

//    size_t num_indices = total_time;

//    for(int i = 0; i < model.num_vertices; i++) {
//        int vertex_idx = i * 4;
//        if(model.vertices[vertex_idx + 1] > 0.9) {
//            printf("Vertex %d has a big Y %.2f\n", i, model.vertices[vertex_idx + 1]);
//        }
//    }
//    printf("\n");

    // TODO: Because we're just drawing the quad here all of the indices start from 0, I think in the end these will
    // need to go back to being relative to the entire vertex array.
    glDrawElements(GL_TRIANGLES, model.num_indices * 3, GL_UNSIGNED_INT, model.indices);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glutSwapBuffers();
    glutPostRedisplay();
}

void load_shader_program() {
    // Compile the vertex shader.
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Log and exit if the compilation failed.
    int compile_success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compile_success);
    if (!compile_success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("ERROR COMPILING VERTEX SHADER: %s\n", infoLog);
        exit(1);
    }

    // Do the same for the fragment shader.
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    shaderProgram = glCreateProgram();

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compile_success);
    if (!compile_success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("ERROR COMPILING FRAGMENT SHADER: %s\n", infoLog);
        exit(1);
    }

    // Attach the vertex and fragment shaders to the shader program.
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    // Link and use the shader program.
    glLinkProgram(shaderProgram);

    // Log and exit if there was a problem with the linking.
    int linkStatus;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("LINKING ERROR: %s\n", infoLog);
        exit(1);
    }

    glUseProgram(shaderProgram);
}

int main(int argc, char** argv) {
    last_frame_time = get_current_time();

    glutInit(&argc, argv);
    glutCreateWindow("Jacks 3-Dimensional Wonderland");
    glutDisplayFunc(display);

    // This enables z-buffering so pixels are occluded based on depth.
    glEnable(GLUT_DOUBLE| GL_DEPTH_TEST);

    // Enable backface culling and set the winding order to counter clockwise.
//    glEnable(GL_CULL_FACE);
//    glCullFace(GL_BACK);
//    glFrontFace(GL_CCW);

    const char* version = (const char*)glGetString(GL_VERSION);
    int major, minor;
    sscanf(version, "%d.%d", &major, &minor);
    printf("OpenGL version supported by your graphics card: %s\n", version);

    load_shader_program();

    init_textures(&technicolor_texture);

    // Read and parse model file.
    char* model_path = "/Users/jack/workspace/3d/models/ship_model.gltf";
//    char* model_path = "/Users/jack/workspace/3d/models/cube.gltf";
    FILE* file = fopen(model_path, "r");
    if(!file) {
       printf("Failed to open model file.");
       exit(-1);
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_buffer = (char*)malloc(file_size);

    size_t bytes_read = fread(json_buffer, 1, file_size, file);
    if(bytes_read != file_size) {
        printf("failed to read entire model file(%lu out of an expected %lu).\n", bytes_read, file_size);
        exit(-1);
    }
    fclose(file);

    cJSON* json = cJSON_ParseWithLength(json_buffer, file_size);
    if(!json) {
        const char* error_str = cJSON_GetErrorPtr();
        if(error_str != NULL) {
            printf("Failed to parse model file: %s\n", error_str);
        }
        cJSON_Delete(json);
        free(json_buffer);
        exit(-1);
    }
    free(json_buffer);

    // TODO: Check that the JSON structure is as expected as we go.
    cJSON* model_buffers = cJSON_GetObjectItem(json, "buffers");
    cJSON* model_buffer  = cJSON_GetArrayItem(model_buffers, 0);
    cJSON* model_uri     = cJSON_GetObjectItem(model_buffer, "uri");
    char* model_uri_base64 = cJSON_GetStringValue(model_uri);

    size_t model_data_size;
    // TODO: Skip the prefix a bit more robustly.
    // TODO: I think the gltf tells us how big we can expect the output buffer to be. Use this to allocate up front?
    char* model_data = base64_decode(&model_uri_base64[37], strlen(model_uri_base64) - 37, &model_data_size);
    if(model_data == NULL) {
        printf("Failed to parse model's base64 data.\n");
        exit(-1);
    }
    cJSON_Delete(json);

    /**
     * Notes on parsing .gltf files correctly: The "buffers" define large portions of data that are accessed
     * different ways for different things(vertices, texture UVs, etc). If we look in "meshes" we can see how
     * to access the different attributes(POSITION=vertices, TEXCOORD=uvs, indices=indices, etc). Each attribute
     * points to an "accessor". The accessor tells us how many items we can expect, what type the items
     * are(5126=float, 5123=unsigned short, etc), etc. The accessors point to a "bufferView" which in turn tell
     * us how to actually pull that type of data out of the big data buffer(offset, stride, length, etc).
     *
     * We should take all of this into account but I'm going to hardcode everything I can to start with. For example
     * the offset and length values here are currently hardcoded from reading the gltf file with my human eyes.
     */
    printf("System float size: %lu\n", sizeof(GLfloat));
    printf("System short size: %lu\n", sizeof(unsigned short));


    model.texture_id = technicolor_texture;
    model.position.x = 0;
    model.position.y = 0;
    model.position.z = 0;

    size_t num_bytes = 1872; // For ship model.
//    size_t num_bytes = 288; // For cube model.
    size_t num_floats = num_bytes / sizeof(GLfloat);
    model.num_vertices = num_floats / 3;
    printf("%d vertices in model\n", model.num_vertices);
    model.vertices = malloc(model.num_vertices * 4 * sizeof(GLfloat));
    model.texture_uvs = malloc(model.num_vertices * 2 * sizeof(GLfloat));
    GLfloat* vertex_data = (GLfloat*)model_data;
    for(int i = 0; i < model.num_vertices; i++) {
        int output_idx = i * 4;
        // The gltf format does not include the w property of the vector.
        int input_idx = i * 3;
        model.vertices[output_idx + 0] = vertex_data[input_idx + 0];
        model.vertices[output_idx + 1] = vertex_data[input_idx + 1];
        model.vertices[output_idx + 2] = vertex_data[input_idx + 2];
        model.vertices[output_idx + 3] = 1.0f;

        // TODO: Read texture UVs from file.
        int uv_idx = i * 2;
        model.texture_uvs[uv_idx]     = (float)rand() / (float)RAND_MAX;
        model.texture_uvs[uv_idx + 1] = (float)rand() / (float)RAND_MAX;
    }
//    print_float_buffer(model.vertices, model.num_vertices * 4, 4);
//    printf("\n=======================\n");

    num_bytes = 456; // For ship.
    size_t indices_offset = 4992; // For ship.
    size_t num_shorts = num_bytes / sizeof(unsigned short);
    model.num_indices = num_shorts / 3;
    printf("%d indices in model\n", model.num_indices);
    model.indices = malloc(num_shorts * sizeof(GLuint));
    unsigned short* index_data = (unsigned short*)(model_data + indices_offset);
    for(int i = 0; i < num_shorts; i++) {
        model.indices[i] = (GLuint)index_data[i];
//        printf("%d:%d, ", model.indices[i], (GLuint)index_data[i]);
    }
//    printf("\n");

    free(model_data);



    // TODO: Should really only need a single allocator.
    vertex_allocator     = new_allocator(sizeof(GLfloat) * 4, 1024);
    index_allocator      = new_allocator(sizeof(GLuint) * 3, 1024);
    texture_uv_allocator = new_allocator(sizeof(GLfloat) * 3, 1024);

//    pyramid1 = create_pyramid(0, 0, technicolor_texture);
//    pyramid = create_pyramid(0, 0);
//    cube = create_cube(0.5f);
//    quad = create_quad(0.8f, 0.4f, technicolor_texture);
//    quad = create_pyramid(0.8f, 0.4f, technicolor_texture);
//    quad = create_cube(0.8f, technicolor_texture);

    vec3_t distance = {.x = 0, .y = 0, .z = 0.7f};
    translate_object(&model, distance);
//
//    distance.x = 0.5;
//    distance.y = 0.5;
//    translate_object(&cube, distance);


    // Create a vertex array object that we can use for assigning the vertex attribute arrays.
    glGenVertexArraysAPPLE(1, &gl_vertex_array_object);
    glBindVertexArrayAPPLE(gl_vertex_array_object);

    // TODO: Pretty sure this is allocating on every draw! Fix immediately!
    // Setup the json_buffer for storing vertex data and assign it to the appropriate input in the shader.
    glGenBuffers(1, &gl_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, model.num_vertices * 4 * sizeof(GLfloat), model.vertices, GL_STATIC_DRAW);
    GLint posAttrib = glGetAttribLocation(shaderProgram, "aPos");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 4, GL_FLOAT, GL_FALSE, 0, 0);

    // Do the same but for the texture UVs.
    glGenBuffers(1, &gl_texture_uv_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, gl_texture_uv_buffer);
    glBufferData(GL_ARRAY_BUFFER, model.num_vertices * 2 * sizeof(GLfloat), model.texture_uvs, GL_STATIC_DRAW);
    GLint texAttrib = glGetAttribLocation(shaderProgram, "aTexCoord");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);



    glutMainLoop();

    return 0;
}

