#ifdef _WIN32
        __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;           // Optimus: force switch to discrete GPU
        __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;     // AMD
#endif

typedef struct 
{
    f32 x, y;     
    f32 r, g, b;
    f32 s, t;
} vertex_t;

typedef struct 
{
    GLuint vao;
    GLuint vbo;
    GLuint shader_program;
    i32    vertex_count;
} render_obj_t;

void monitors_info()
{
    int monitor_count;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    for (int i = 0; i < monitor_count; i++) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        printf("Monitor %d: %dx%d @%dHz\n", i, mode->width, mode->height, mode->refreshRate);
    }
}

void set_clipboard(char *txt)
{
    glfwSetClipboardString(gc.window, txt);
}

char *get_clipboard()
{
    return glfwGetClipboardString(gc.window);
}

void custom_cursor(char *path)
{
    int cursor_width, cursor_height;
    unsigned char* cursor_pixels = stbi_load(path, &cursor_width, &cursor_height, NULL, 4);
    GLFWimage cursor_image = {cursor_width, cursor_height, cursor_pixels};
    GLFWcursor* custom_cursor = glfwCreateCursor(&cursor_image, 0, 0);
    glfwSetCursor(gc.window, custom_cursor);
    stbi_image_free(cursor_pixels);
}

void set_window_icon(char *path)
{
    GLFWimage icon;
    icon.pixels = stbi_load(path, &icon.width, &icon.height, NULL, 4);
    if (icon.pixels) {
        glfwSetWindowIcon(gc.window, 1, &icon);
        stbi_image_free(icon.pixels);
    } else {
        fprintf(stderr, "Failed to load window icon\n");
    }
}

void set_dark_mode(GLFWwindow *window)
{
    #ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(window);
        if (!hwnd){
            return;
        }
        BOOL value = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    #endif
}

void *create_window(u32 width, u32 height, const char *title)
{
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return NULL;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 8); // Enable multisampling for smoother edges
    
    GLFWwindow *window = CHECK_PTR(glfwCreateWindow((int)width, (int)height, title, NULL, NULL));
    
    glfwMakeContextCurrent(window);
    
    glViewport(0, 0, (int)width, (int)height);
    
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetErrorCallback(error_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowRefreshCallback(window, window_refresh_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetDropCallback(window, drop_callback);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW initialization error: %s\n", glewGetErrorString(err));
        return NULL;
    }
    
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glEnable(GL_DEPTH_TEST);

    glfwSwapInterval(1);

    monitors_info();
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    gc.hand_cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    gc.regular_cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    return window;
}


void init_framebuffer(void)
{
    glGenTextures(1, &gc.texture);
    glBindTexture(GL_TEXTURE_2D, gc.texture);

    // Generate a full screen texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
                 (int)1920, (int)1080,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &gc.read_fbo);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gc.read_fbo);

    // attach the texture to the framebuffer
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gc.texture, 0);
}

void blit_to_screen(void *pixels, u32 width, u32 height)
{
    glBindTexture(GL_TEXTURE_2D, gc.texture);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    (int)width, (int)height,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gc.read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // make the default framebuffer active again ?
    
    // transfer pixel values from one region of a read framebuffer to another region of a draw framebuffer.
    glBlitFramebuffer(0, 0, width, height,  // src
                      0, height, width, 0,  // dst (flipped)    
                      GL_COLOR_BUFFER_BIT,    
                      GL_NEAREST);      
}

void draw_render_object(render_obj_t *obj, vertex_t *vertices)
{
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, obj->vertex_count * sizeof(vertex_t), vertices);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw
    glBindVertexArray(obj->vao);
    glDrawArrays(GL_TRIANGLES, 0, obj->vertex_count);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
}

void draw_render_object_wireframe(render_obj_t *obj, vertex_t *vertices) 
{
    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, obj->vertex_count * sizeof(vertex_t), vertices);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Enable line smoothing for better quality
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    glBindVertexArray(obj->vao);
    glDrawArrays(GL_LINES, 0, obj->vertex_count);
    glBindVertexArray(0);
    
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
}

void draw_render_object_line_strip(render_obj_t *obj, vertex_t *vertices, f32 line_width) 
{
    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, obj->vertex_count * sizeof(vertex_t), vertices);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    glLineWidth(line_width);

    glBindVertexArray(obj->vao);
    glDrawArrays(GL_LINE_STRIP, 0, obj->vertex_count);
    glBindVertexArray(0);
    
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);

    glLineWidth(1.0f); // Reset line width
}

void draw_render_object_line_loop(render_obj_t *obj, vertex_t *vertices, f32 line_width) 
{
    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, obj->vertex_count * sizeof(vertex_t), vertices);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    glLineWidth(line_width);

    glBindVertexArray(obj->vao);
    glDrawArrays(GL_LINE_LOOP, 0, obj->vertex_count);
    glBindVertexArray(0);
    
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);

    glLineWidth(1.0f); // Reset line width
}


void init_render_object(render_obj_t* obj, i32 max_vertices, char* vs, char* fs) 
{
    char* vertexShaderSource   = read_shader_source(vs); 
    char* fragmentShaderSource = read_shader_source(fs); 

    obj->shader_program = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    
    glGenVertexArrays(1, &obj->vao);
    glGenBuffers(1, &obj->vbo);
    
    glBindVertexArray(obj->vao);
    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    
    glBufferData(GL_ARRAY_BUFFER, max_vertices * sizeof(vertex_t), NULL, GL_DYNAMIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void*)offsetof(vertex_t, x));
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void*)offsetof(vertex_t, r));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    obj->vertex_count = 0;
}

render_obj_t init_rect(char* vs, char* fs)
{
    f32 vertices[] = 
    {
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.5f,  0.5f,

        -0.5f, -0.5f,
        0.5f,  0.5f,
        -0.5f,  0.5f
    };

    char* vertexShaderSource   = read_shader_source(vs); 
    char* fragmentShaderSource = read_shader_source(fs); 

    render_obj_t rect;
    
    rect.vertex_count = NUM_ELEMS(vertices);
    rect.shader_program = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    
    glGenVertexArrays(1, &rect.vao);
    glGenBuffers(1, &rect.vbo);
    
    glBindVertexArray(&rect.vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, &rect.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return rect;
}

char* read_shader_source(const char* filepath) 
{
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        printf("ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ\n");
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer and read file
    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    
    fclose(file);
    return buffer;
}

GLuint compile_shader(GLenum type, const char* source) 
{
    GLuint shader = glCreateShader(type);

    // attach shader source code to the shader object
    glShaderSource(shader, 1, &source, NULL);
    // compile the shader
    glCompileShader(shader);

    // Check for compilation errors
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "Shader compilation error:\n%s\n", infoLog);
    }

    return shader;
}

// Create a shader program from vertex and fragment shaders
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) 
{
    GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, fragmentSource);

    // Link both shaders into a shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        fprintf(stderr, "Shader program linking error:\n%s\n", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void setBool(unsigned int ID, const char* name, int value) 
{         
    glUniform1i(glGetUniformLocation(ID, name), value); 
}

void setInt(unsigned int ID, const char* name, int value) 
{ 
    glUniform1i(glGetUniformLocation(ID, name), value); 
}

void setFloat(unsigned int ID, const char* name, float value)
{ 
    glUniform1f(glGetUniformLocation(ID, name), value); 
}

void setFloat2(unsigned int ID, const char* name, float value1, float value2)
{ 
    glUniform2f(glGetUniformLocation(ID, name), value1, value2); 
}

void setVec3(unsigned int ID, const char* name, vec3f_t *vector) 
{
    unsigned int loc = glGetUniformLocation(ID, name);
    glUniform3fv(loc, 1, vector);
}

void setMat4(unsigned int ID, const char* name, mat4x4_t *matrix) 
{
    unsigned int loc = glGetUniformLocation(ID, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, matrix);
}

#define TEXTURE_PATH_MAX 256
#define TEXTURE_TYPE_MAX 64
#define TEXTURE_UNIFORM_MAX 64

typedef struct Texture {
    GLuint id;
    char type[TEXTURE_TYPE_MAX];
    char path[TEXTURE_PATH_MAX];
    char uniform[TEXTURE_UNIFORM_MAX];
    int width, height, nrChannels;
} Texture;

Texture* texture_create(const char* texturePath, const char* uniform) 
{
    Texture* texture = (Texture*)malloc(sizeof(Texture));
    if (!texture) {
        fprintf(stderr, "Failed to allocate memory for texture\n");
        return NULL;
    }
    
    texture->id = 0;
    texture->width = 0;
    texture->height = 0;
    texture->nrChannels = 0;
    
    strncpy(texture->uniform, uniform, TEXTURE_UNIFORM_MAX - 1);
    texture->uniform[TEXTURE_UNIFORM_MAX - 1] = '\0';
    
    strncpy(texture->path, texturePath, TEXTURE_PATH_MAX - 1);
    texture->path[TEXTURE_PATH_MAX - 1] = '\0';
    
    texture->type[0] = '\0'; 
    
    texture_from_file(texture, texturePath);
    
    return texture;
}

Texture* texture_create_simple(const char* texturePath)
 {
    Texture* texture = (Texture*)malloc(sizeof(Texture));
    if (!texture) {
        fprintf(stderr, "Failed to allocate memory for texture\n");
        return NULL;
    }
    
    // Initialize fields
    texture->id = 0;
    texture->width = 0;
    texture->height = 0;
    texture->nrChannels = 0;
    texture->uniform[0] = '\0'; // No uniform name
    
    // Copy path
    strncpy(texture->path, texturePath, TEXTURE_PATH_MAX - 1);
    texture->path[TEXTURE_PATH_MAX - 1] = '\0';
    
    texture->type[0] = '\0'; // Initialize type as empty
    
    // Load texture
    texture_from_file(texture, texturePath);
    
    return texture;
}

GLuint texture_from_file(Texture* texture, const char* texturePath) 
{
    printf("Loading texture from: %s\n", texturePath);
    
    if (!texture) {
        fprintf(stderr, "Invalid texture pointer\n");
        return 0;
    }

    // Generate texture ID
    glGenTextures(1, &texture->id);

    // Load image
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(texturePath, &texture->width, &texture->height, 
                                   &texture->nrChannels, 0);

    if (data) 
    {
        GLenum format;
        switch (texture->nrChannels) 
        {
            case 1: format = GL_RED; break;
            case 2: format = GL_RG; break;
            case 3: format = GL_RGB; break;
            case 4: format = GL_RGBA; break;
            default: format = GL_RGB; // Fallback to RGB if unknown
        }
        
        glBindTexture(GL_TEXTURE_2D, texture->id);
        glTexImage2D(GL_TEXTURE_2D, 0, format, texture->width, texture->height, 
                     0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Set texture wrapping and filtering options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // Filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        fprintf(stderr, "Failed to load texture: %s\n", texturePath);
    }
    
    stbi_image_free(data);
    return texture->id;
}

void texture_bind(const Texture* texture, GLenum textureUnit) 
{
    if (!texture) return;
    
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, texture->id);
}

void texture_use(const Texture* texture, GLuint shaderProgram, unsigned int textureUnit) 
{
    if (!texture) return;
    
    // Only set uniform if it's not empty
    if (texture->uniform[0] != '\0') {
        setInt(shaderProgram, texture->uniform, textureUnit);
    }
    
    GLenum glTextureUnit = GL_TEXTURE0 + textureUnit;
    texture_bind(texture, glTextureUnit);
}

void texture_destroy(Texture* texture) 
{
    if (texture) {
        if (texture->id != 0) {
            glDeleteTextures(1, &texture->id);
        }
        free(texture);
    }
}
