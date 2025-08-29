#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <math.h>

#define PORT 8080
#define MAX_USERNAME_LENGTH 64
#define MAX_PASSWORD_LENGTH 64
#define MAX_QUESTIONS 100
#define MAX_QUESTION_LENGTH 1024
#define MAX_OPTION_LENGTH 256
#define MAX_ANSWER_LENGTH 64
#define MAX_EXPLANATION_LENGTH 1024
#define MAX_POST_SIZE 1024
#define FRONTEND_PATH "../frontend"  // Path to frontend directory relative to backend
#define HASH_TABLE_SIZE 101 // Prime number for hash table size

// Data Structures 
typedef struct Question {
    int id;
    char question[MAX_QUESTION_LENGTH];
    char options[4][MAX_OPTION_LENGTH];
    int correct_answer;
    char explanation[MAX_EXPLANATION_LENGTH];
    int difficulty; // 1-10 scale for priority queue
    struct Question *next; // For linked list
} Question;

// Binary Search Tree Node for Questions
typedef struct BSTNode {
    Question *question;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;

// User Authentication Hash Table Entry
typedef struct AuthEntry {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    struct AuthEntry *next; // For collision handling (chaining)
} AuthEntry;

// Priority Queue Node based on question difficulty
typedef struct PQNode {
    Question *question;
    struct PQNode *next;
} PQNode;

typedef struct {
    char *post_data;
    size_t post_size;
} ConnectionInfo;

// Global variables
Question *question_head = NULL; // Original linked list
BSTNode *question_bst_root = NULL; // BST for faster question lookup
AuthEntry *auth_hash_table[HASH_TABLE_SIZE] = {NULL}; // Hash table for auth
PQNode *priority_queue_head = NULL; // Priority queue for questions by difficulty

// Forward declarations
static int authenticate(const char *username, const char *password);
static void cleanup_connection_info(void **con_cls);
static int parse_post_data(const char* data, char* username, char* password);
static void load_questions(void);
static void load_auth_data(void);
static BSTNode* insert_bst(BSTNode *root, Question *question);
static Question* search_bst(BSTNode *root, int id);
static unsigned int hash_string(const char *str);
static void insert_auth_entry(const char *username, const char *password);
static int check_auth_hash_table(const char *username, const char *password);
static void insert_priority_queue(Question *question);
static Question* get_next_priority_question(void);
static void free_all_data_structures(void);
static void free_bst(BSTNode *node);
static void copy_to_temp_queue(Question *q, PQNode **temp_queue_head);
static enum MHD_Result handle_login(struct MHD_Connection *connection, 
                                   const char *upload_data,
                                   size_t *upload_data_size,
                                   void **con_cls);
static struct MHD_Response* create_response(const char *content, const char *content_type);

// Hash function for username
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381; // Initial value (arbitrary prime)
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % HASH_TABLE_SIZE;
}

// Insert into auth hash table
static void insert_auth_entry(const char *username, const char *password) {
    if (!username || !password) return;
    
    unsigned int index = hash_string(username);
    
    // Create new entry
    AuthEntry *new_entry = (AuthEntry*)malloc(sizeof(AuthEntry));
    if (!new_entry) return;
    
    strncpy(new_entry->username, username, MAX_USERNAME_LENGTH - 1);
    new_entry->username[MAX_USERNAME_LENGTH - 1] = '\0';
    
    strncpy(new_entry->password, password, MAX_PASSWORD_LENGTH - 1);
    new_entry->password[MAX_PASSWORD_LENGTH - 1] = '\0';
    
    // Insert at beginning of chain (simplest approach)
    new_entry->next = auth_hash_table[index];
    auth_hash_table[index] = new_entry;
    
    printf("Added user %s to hash table at index %u\n", username, index);
}

// Check auth using hash table
static int check_auth_hash_table(const char *username, const char *password) {
    if (!username || !password) return 0;
    
    unsigned int index = hash_string(username);
    AuthEntry *current = auth_hash_table[index];
    
    while (current) {
        if (strcmp(current->username, username) == 0 && 
            strcmp(current->password, password) == 0) {
            return 1; // Found match
        }
        current = current->next;
    }
    
    return 0; // No match found
}

// BST insertion
static BSTNode* insert_bst(BSTNode *root, Question *question) {
    if (root == NULL) {
        BSTNode *new_node = (BSTNode*)malloc(sizeof(BSTNode));
        if (!new_node) return NULL;
        
        new_node->question = question;
        new_node->left = NULL;
        new_node->right = NULL;
        return new_node;
    }
    
    // Use question ID as key for BST
    if (question->id < root->question->id) {
        root->left = insert_bst(root->left, question);
    } else if (question->id > root->question->id) {
        root->right = insert_bst(root->right, question);
    }
    
    return root;
}

// BST search
static Question* search_bst(BSTNode *root, int id) {
    if (root == NULL) return NULL;
    
    if (id == root->question->id) {
        return root->question;
    } else if (id < root->question->id) {
        return search_bst(root->left, id);
    } else {
        return search_bst(root->right, id);
    }
}

// Insert into priority queue (based on difficulty)
static void insert_priority_queue(Question *question) {
    PQNode *new_node = (PQNode*)malloc(sizeof(PQNode));
    if (!new_node) return;
    
    new_node->question = question;
    
    // Empty queue or new question has higher priority (difficulty)
    if (priority_queue_head == NULL || question->difficulty > priority_queue_head->question->difficulty) {
        new_node->next = priority_queue_head;
        priority_queue_head = new_node;
    } else {
        // Find position to insert
        PQNode *current = priority_queue_head;
        while (current->next != NULL && 
               current->next->question->difficulty >= question->difficulty) {
            current = current->next;
        }
        
        new_node->next = current->next;
        current->next = new_node;
    }
}

// Get highest priority question and remove from queue
static Question* get_next_priority_question(void) {
    if (priority_queue_head == NULL) return NULL;
    
    PQNode *top = priority_queue_head;
    Question *question = top->question;
    priority_queue_head = priority_queue_head->next;
    
    free(top);
    return question;
}

// Function to cleanup all allocated data structures
static void free_all_data_structures(void) {
    // Free linked list
    Question *q_current = question_head;
    while (q_current) {
        Question *temp = q_current;
        q_current = q_current->next;
        free(temp);
    }
    
    // Free BST
    free_bst(question_bst_root);
    
    // Free hash table entries
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        AuthEntry *current = auth_hash_table[i];
        while (current) {
            AuthEntry *temp = current;
            current = current->next;
            free(temp);
        }
    }
    
    // Free priority queue
    PQNode *pq_current = priority_queue_head;
    while (pq_current) {
        PQNode *temp = pq_current;
        pq_current = pq_current->next;
        free(temp);
    }
}

// Load authentication data from file into hash table
static void load_auth_data(void) {
    FILE *fp = fopen("../backend/auth.txt", "r");
    if (!fp) {
        fp = fopen("backend/auth.txt", "r");
        if (!fp) {
            fp = fopen("auth.txt", "r");
            if (!fp) {
                printf("Could not open auth file\n");
                return;
            }
        }
    }
    
    char line[MAX_USERNAME_LENGTH + MAX_PASSWORD_LENGTH + 2]; // +2 for ":" and "\0"
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // Split by colon
        char *colon = strchr(line, ':');
        if (!colon) continue;
        
        *colon = '\0'; // Split string
        char *username = line;
        char *password = colon + 1;
        
        insert_auth_entry(username, password);
    }
    
    fclose(fp);
    printf("Authentication data loaded into hash table\n");
}

// Function to parse POST data
static int parse_post_data(const char* data, char* username, char* password) {
    if (!data || !username || !password) return 0;
    
    char *user_start = strstr(data, "username=");
    char *pass_start = strstr(data, "password=");
    
    if (!user_start || !pass_start) return 0;
    
    user_start += 9;  // Skip "username="
    pass_start += 9;  // Skip "password="
    
    char *user_end = strchr(user_start, '&');
    char *pass_end = strchr(pass_start, '&');
    
    if (!user_end) user_end = user_start + strlen(user_start);
    if (!pass_end) pass_end = pass_start + strlen(pass_start);
    
    size_t user_len = user_end - user_start;
    size_t pass_len = pass_end - pass_start;
    
    if (user_len >= MAX_USERNAME_LENGTH || pass_len >= MAX_PASSWORD_LENGTH) return 0;
    
    strncpy(username, user_start, user_len);
    strncpy(password, pass_start, pass_len);
    username[user_len] = '\0';
    password[pass_len] = '\0';
    
    return 1;
}

// Clean up connection info
static void cleanup_connection_info(void **con_cls) {
    if (*con_cls) {
        ConnectionInfo *con_info = *con_cls;
        if (con_info->post_data)
            free(con_info->post_data);
        free(con_info);
        *con_cls = NULL;
    }
}

// Function to copy questions file to current directory
static void ensure_questions_file() {
    const char* source_paths[] = {
        "../backend/questions.txt",
        "backend/questions.txt",
        "questions.txt"
    };
    
    // First check if file exists in current directory
    FILE* fp = fopen("./questions.txt", "r");
    if (fp) {
        printf("Questions file already exists in current directory\n");
        fclose(fp);
        return;
    }
    
    // Try to copy from source paths
    for (int i = 0; i < sizeof(source_paths)/sizeof(source_paths[0]); i++) {
        FILE* src = fopen(source_paths[i], "r");
        if (!src) continue;
        
        FILE* dst = fopen("./questions.txt", "w");
        if (!dst) {
            fclose(src);
            continue;
        }
        
        char buffer[1024];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }
        
        fclose(src);
        fclose(dst);
        printf("Successfully copied questions file from %s\n", source_paths[i]);
        return;
    }
    
    printf("WARNING: Could not copy questions file to current directory\n");
}

// Function to read questions file
static char* read_questions_file() {
    ensure_questions_file();  // Make sure file exists in current directory
    
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    }
    
    FILE* fp = fopen("./questions.txt", "r");
    if (!fp) {
        printf("ERROR: Could not open questions.txt in current directory: %s\n", strerror(errno));
        return NULL;
    }
    
    printf("Successfully opened questions.txt\n");
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    printf("File size: %ld bytes\n", size);
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("ERROR: Failed to allocate memory\n");
        fclose(fp);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, size, fp);
    buffer[read] = '\0';
    fclose(fp);
    
    printf("Read %zu bytes from file\n", read);
    printf("First line: %.*s\n", (int)(strchr(buffer, '\n') - buffer), buffer);
    
    return buffer;
}

// Load Questions from File 
static void load_questions() {
    printf("\n=== Loading Questions ===\n");
    
    // Read the questions file directly
    FILE *file = fopen("../backend/questions.txt", "r");
    if (!file) {
        file = fopen("backend/questions.txt", "r");
        if (!file) {
            file = fopen("questions.txt", "r");
            if (!file) {
                printf("Failed to open questions.txt: %s\n", strerror(errno));
                return;
            }
        }
    }
    
    // Reset all data structures
    question_head = NULL;
    question_bst_root = NULL;
    priority_queue_head = NULL;
    Question *last = NULL;
    int count = 0;
    
    char line[2048];
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        
        // Skip empty lines
        if (len == 0) continue;
        
        // Create new question
        Question *new_question = malloc(sizeof(Question));
        if (!new_question) {
            printf("Failed to allocate memory for question\n");
            continue;
        }
        memset(new_question, 0, sizeof(Question));
        
        // Parse line using pipe delimiter
        char *token;
        char *rest = line;
        
        // Parse ID
        token = strtok_r(rest, "|", &rest);
        if (!token) {
            printf("Error: Missing ID in line: %s\n", line);
            free(new_question);
            continue;
        }
        new_question->id = atoi(token);
        printf("ID: %d\n", new_question->id);
        
        // Parse question text
        token = strtok_r(rest, "|", &rest);
        if (!token) {
            printf("Error: Missing question text in line: %s\n", line);
            free(new_question);
            continue;
        }
        strncpy(new_question->question, token, sizeof(new_question->question) - 1);
        printf("Text: %s\n", new_question->question);
        
        // Parse options (4 options)
        for (int i = 0; i < 4; i++) {
            token = strtok_r(rest, "|", &rest);
            if (!token) {
                printf("Error: Missing option %d in line: %s\n", i+1, line);
                free(new_question);
                continue;
            }
            strncpy(new_question->options[i], token, sizeof(new_question->options[i]) - 1);
            printf("Option %d: %s\n", i+1, new_question->options[i]);
        }
        
        // Parse correct answer (1-based in file, convert to 0-based)
        token = strtok_r(rest, "|", &rest);
        if (!token) {
            printf("Error: Missing correct answer in line: %s\n", line);
            free(new_question);
            continue;
        }
        new_question->correct_answer = atoi(token) - 1; // Convert to 0-based
        printf("Correct answer: %d\n", new_question->correct_answer + 1);
        
        // Parse explanation
        token = strtok_r(rest, "|", &rest);
        if (token) {
            strncpy(new_question->explanation, token, sizeof(new_question->explanation) - 1);
            printf("Explanation: %s\n", new_question->explanation);
        } else {
            strncpy(new_question->explanation, "No explanation provided", sizeof(new_question->explanation) - 1);
        }
        
        // Set difficulty level based on question ID for now (could be more sophisticated)
        new_question->difficulty = (new_question->id % 10) + 1; // 1-10 difficulty scale
        
        // Add to linked list
        new_question->next = NULL;
        if (question_head == NULL) {
            question_head = new_question;
        } else {
            last->next = new_question;
        }
        last = new_question;
        
        // Add to BST
        question_bst_root = insert_bst(question_bst_root, new_question);
        
        // Add to priority queue
        insert_priority_queue(new_question);
        
        count++;
    }
    
    fclose(file);
    printf("Loaded %d questions\n", count);
    
    if (count == 0) {
        printf("WARNING: No questions were loaded!\n");
    } else {
        printf("Data structures populated: Linked List, Binary Search Tree, Priority Queue\n");
    }
}

// Authentication against auth.txt
static int authenticate(const char *username, const char *password) {
    if (!username || !password) return 0;
    
    // Use our hash table for authentication
    return check_auth_hash_table(username, password);
}

// Create HTTP response with content and content type
static struct MHD_Response* create_response(const char *content, const char *content_type) {
    if (!content) {
        return MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    }
    
    size_t len = strlen(content);
    char *copy = malloc(len + 1);
    if (!copy) {
        printf("Failed to allocate memory for response\n");
        return MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    }
    
    strcpy(copy, content);
    struct MHD_Response *response = MHD_create_response_from_buffer(len, copy, MHD_RESPMEM_MUST_FREE);
    
    if (!response) {
        free(copy);
        return MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    }
    
    MHD_add_response_header(response, "Content-Type", content_type);
    return response;
}

// Function to get content type based on file extension
const char* get_content_type(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return "text/plain";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    if (strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    return "text/plain";
}

// Function to serve static files
static enum MHD_Result serve_file(struct MHD_Connection *connection, const char *url) {
    struct MHD_Response *response;
    enum MHD_Result ret;
    char full_path[1024];
    char cwd[1024];
    
    // Get current working directory for debugging
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    } else {
        printf("Failed to get current working directory\n");
    }
    
    // Default to index.html for root URL
    if (strcmp(url, "/") == 0) {
        url = "/index.html";
    }
    
    // Remove leading slash and construct full path
    const char *file_path = (*url == '/') ? url + 1 : url;
    snprintf(full_path, sizeof(full_path), "%s/%s", FRONTEND_PATH, file_path);
    
    printf("URL requested: %s\n", url);
    printf("File path: %s\n", file_path);
    printf("Full path: %s\n", full_path);
    
    // Check if file exists and is readable
    FILE *file = fopen(full_path, "rb");
    if (!file) {
        printf("File not found: %s (errno: %d - %s)\n", full_path, errno, strerror(errno));
        goto send_404;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    printf("File size: %ld bytes\n", file_size);
    
    char *file_data = malloc(file_size);
    if (!file_data) {
        fclose(file);
        printf("Memory allocation failed for file: %s\n", full_path);
        goto send_404;
    }
    
    if (fread(file_data, 1, file_size, file) != (size_t)file_size) {
        free(file_data);
        fclose(file);
        printf("Failed to read file: %s\n", full_path);
        goto send_404;
    }
    
    fclose(file);
    
    // Create response
    response = MHD_create_response_from_buffer(file_size, file_data, MHD_RESPMEM_MUST_FREE);
    if (!response) {
        free(file_data);
        printf("Failed to create response for file: %s\n", full_path);
        return MHD_NO;
    }
    
    MHD_add_response_header(response, "Content-Type", get_content_type(full_path));
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    printf("File served successfully: %s\n", full_path);
    return ret;

send_404:
    response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    if (!response) {
        printf("Failed to create 404 response\n");
        return MHD_NO;
    }
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    printf("404 response sent for: %s\n", url);
    return ret;
}

// Get a specific question by ID using BST
static char* get_question_by_id_json(int id) {
    Question *q = search_bst(question_bst_root, id);
    if (!q) {
        return strdup("{\"error\":\"Question not found\"}");
    }
    
    char *json;
    asprintf(&json, 
        "{\"id\":%d,\"text\":\"%s\",\"options\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"correct\":%d,\"explanation\":\"%s\",\"difficulty\":%d}",
        q->id, q->question, 
        q->options[0], q->options[1], q->options[2], q->options[3],
        q->correct_answer,
        q->explanation,
        q->difficulty
    );
    
    return json;
}

// Handle GET /api/questions endpoint
static enum MHD_Result handle_get_questions(struct MHD_Connection *connection) {
    struct MHD_Response *response;
    enum MHD_Result ret;
    
    // Check if we have questions
    if (!question_head) {
        const char *error_msg = "{\"error\":\"No questions available\"}";
        response = create_response(error_msg, "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Check for query parameter id
    const char *id_param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "id");
    
    if (id_param) {
        // Return a specific question using BST for efficient lookup
        int id = atoi(id_param);
        char *json = get_question_by_id_json(id);
        
        response = create_response(json, "application/json");
        free(json);
    } else {
        // Build formatted text with questions, one per line
        // Format: id|question|option1|option2|option3|option4|correct|explanation
        size_t buffer_size = 1024 * 1024; // 1MB buffer
        char *buffer = malloc(buffer_size);
        if (!buffer) {
            return MHD_NO;
        }
        
        int offset = 0;
        Question *current = question_head;
        
        while (current != NULL && offset < buffer_size - 1024) {
            // Format each question as pipe-delimited text
            // Note: Adding 1 to correct_answer to convert from 0-based to 1-based
            offset += snprintf(buffer + offset, buffer_size - offset,
                "%d|%s|%s|%s|%s|%s|%d|%s\n",
                current->id, 
                current->question,
                current->options[0], 
                current->options[1], 
                current->options[2], 
                current->options[3],
                current->correct_answer + 1, // Convert to 1-based for frontend
                current->explanation
            );
            
            current = current->next;
        }
        
        // Ensure buffer is null-terminated
        if (offset < buffer_size) {
            buffer[offset] = '\0';
        } else {
            buffer[buffer_size - 1] = '\0';
        }
        
        response = create_response(buffer, "text/plain; charset=utf-8");
        free(buffer);
    }
    
    // Add CORS headers
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
    
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Handle GET /api/priority-questions endpoint to get questions by difficulty
static enum MHD_Result handle_get_priority_questions(struct MHD_Connection *connection) {
    struct MHD_Response *response;
    enum MHD_Result ret;
    
    // Get number of questions requested
    const char *count_param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "count");
    int count = count_param ? atoi(count_param) : 5; // Default to 5 questions
    
    if (count <= 0 || count > 100) {
        count = 5; // Sanitize input
    }
    
    // Create a copy of our priority queue (to avoid modifying the original)
    PQNode *temp_queue_head = NULL;
    
    // Copy each question to our temporary queue
    Question *current = question_head;
    while (current) {
        copy_to_temp_queue(current, &temp_queue_head);
        current = current->next;
    }
    
    // Build JSON array of questions by priority
    size_t buffer_size = 1024 * 1024; // 1MB buffer
    char *json_buffer = malloc(buffer_size);
    if (!json_buffer) {
        // Clean up temp queue
        while (temp_queue_head) {
            PQNode *next = temp_queue_head->next;
            free(temp_queue_head);
            temp_queue_head = next;
        }
        return MHD_NO;
    }
    
    int offset = 0;
    offset += snprintf(json_buffer + offset, buffer_size - offset, "[");
    
    int first = 1;
    int questions_added = 0;
    
    // Get top N questions from priority queue
    while (temp_queue_head && questions_added < count && offset < buffer_size - 1024) {
        if (!first) {
            offset += snprintf(json_buffer + offset, buffer_size - offset, ",");
        } else {
            first = 0;
        }
        
        Question *q = temp_queue_head->question;
        
        offset += snprintf(json_buffer + offset, buffer_size - offset,
            "{\"id\":%d,\"text\":\"%s\",\"options\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"correct\":%d,\"explanation\":\"%s\",\"difficulty\":%d}",
            q->id, q->question, 
            q->options[0], q->options[1], q->options[2], q->options[3],
            q->correct_answer,
            q->explanation,
            q->difficulty
        );
        
        questions_added++;
        
        // Move to next node
        PQNode *to_free = temp_queue_head;
        temp_queue_head = temp_queue_head->next;
        free(to_free);
    }
    
    // Free any remaining nodes
    while (temp_queue_head) {
        PQNode *next = temp_queue_head->next;
        free(temp_queue_head);
        temp_queue_head = next;
    }
    
    offset += snprintf(json_buffer + offset, buffer_size - offset, "]");
    
    response = create_response(json_buffer, "application/json");
    free(json_buffer);
    
    // Add CORS headers
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
    
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Handle login request
static enum MHD_Result handle_login(struct MHD_Connection *connection, 
                                  const char *upload_data,
                                  size_t *upload_data_size,
                                  void **con_cls) {
    static char error_response[] = "{\"success\":false,\"message\":\"Invalid credentials\"}";
    static char success_response[] = "{\"success\":true,\"message\":\"Login successful\"}";
    struct MHD_Response *response;
    enum MHD_Result ret;
    
    if (*con_cls == NULL) {
        ConnectionInfo *con_info = calloc(1, sizeof(ConnectionInfo));
        if (!con_info) return MHD_NO;
        *con_cls = con_info;
        return MHD_YES;
    }
    
    ConnectionInfo *con_info = *con_cls;
    
    if (*upload_data_size != 0) {
        if (con_info->post_size + *upload_data_size > MAX_POST_SIZE) {
            return MHD_NO;
        }
        
        char *new_data = realloc(con_info->post_data, con_info->post_size + *upload_data_size + 1);
        if (!new_data) return MHD_NO;
        
        con_info->post_data = new_data;
        memcpy(con_info->post_data + con_info->post_size, upload_data, *upload_data_size);
        con_info->post_size += *upload_data_size;
        con_info->post_data[con_info->post_size] = '\0';
        
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    char username[MAX_USERNAME_LENGTH] = {0};
    char password[MAX_PASSWORD_LENGTH] = {0};
    
    if (!parse_post_data(con_info->post_data, username, password)) {
        response = MHD_create_response_from_buffer(strlen(error_response),
                                                 (void*)error_response,
                                                 MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
        MHD_destroy_response(response);
        cleanup_connection_info(con_cls);
        return ret;
    }
    
    if (authenticate(username, password)) {
        printf("Login successful for user: %s\n", username);
        response = MHD_create_response_from_buffer(strlen(success_response),
                                                 (void*)success_response,
                                                 MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    } else {
        printf("Login failed for user: %s\n", username);
        response = MHD_create_response_from_buffer(strlen(error_response),
                                                 (void*)error_response,
                                                 MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, response);
    }
    
    MHD_destroy_response(response);
    cleanup_connection_info(con_cls);
    return ret;
}

// Main request handler
static enum MHD_Result handle_request(void *cls,
                                    struct MHD_Connection *connection,
                                    const char *url,
                                    const char *method,
                                    const char *version,
                                    const char *upload_data,
                                    size_t *upload_data_size,
                                    void **con_cls) {
    
    printf("Received %s request for %s\n", method, url);
    
    // Handle CORS preflight request
    if (0 == strcmp(method, "OPTIONS")) {
        struct MHD_Response *response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
        
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
        MHD_add_response_header(response, "Access-Control-Max-Age", "86400");
        
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    if (0 == strcmp(method, "GET")) {
        // Handle GET requests
        
        // API endpoints
        if (0 == strcmp(url, "/api/questions")) {
            return handle_get_questions(connection);
        } 
        else if (0 == strcmp(url, "/api/priority-questions")) {
            return handle_get_priority_questions(connection);
        }
        
        // Static file server
        return serve_file(connection, url);
    }
    
    if (0 == strcmp(method, "POST")) {
        // Handle POST requests
        if (0 == strcmp(url, "/api/login")) {
            return handle_login(connection, upload_data, upload_data_size, con_cls);
        }
    }
    
    // Method not allowed or resource not found
    const char* error_msg = "Not Found";
    struct MHD_Response *response = create_response(error_msg, "text/plain");
    
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Main function
int main(void) {
    printf("\n=== Online Exam Platform Backend Server ===\n");
    printf("Starting server on port %d...\n", PORT);
    
    // Initialize our data structures
    load_auth_data();
    load_questions();
    
    // Example of BST search
    int test_id = 1;
    Question *found = search_bst(question_bst_root, test_id);
    if (found) {
        printf("\nBST Search Test - Found question %d: %s\n", test_id, found->question);
    } else {
        printf("\nBST Search Test - Question %d not found\n", test_id);
    }
    
    // Example of priority queue
    printf("\nPriority Queue Test - Getting highest difficulty questions:\n");
    for (int i = 0; i < 3; i++) {
        Question *q = get_next_priority_question();
        if (q) {
            printf("- Q%d (Difficulty %d): %s\n", q->id, q->difficulty, q->question);
        }
    }
    
    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_POLL_INTERNALLY | MHD_USE_DEBUG | MHD_USE_ERROR_LOG,
        PORT, NULL, NULL, 
        &handle_request, NULL,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
        MHD_OPTION_END
    );
    
    if (NULL == daemon) {
        printf("Failed to start server\n");
        return 1;
    }
    
    printf("Server running. Press ENTER to stop.\n");
    getchar();
    
    printf("Stopping server...\n");
    MHD_stop_daemon(daemon);
    
    // Clean up data structures
    free_all_data_structures();
    
    printf("Server stopped. Goodbye!\n");
    return 0;
}

// Definition of free_bst function
static void free_bst(BSTNode *node) {
    if (node == NULL) return;
    free_bst(node->left);
    free_bst(node->right);
    free(node); // Don't free question as it's shared with linked list
}

// Copy a question to temporary priority queue
static void copy_to_temp_queue(Question *q, PQNode **temp_queue_head) {
    PQNode *new_node = (PQNode*)malloc(sizeof(PQNode));
    if (!new_node) return;
    
    new_node->question = q;
    
    // Empty queue or new question has higher priority
    if (*temp_queue_head == NULL || q->difficulty > (*temp_queue_head)->question->difficulty) {
        new_node->next = *temp_queue_head;
        *temp_queue_head = new_node;
    } else {
        // Find position to insert
        PQNode *current = *temp_queue_head;
        while (current->next != NULL && 
              current->next->question->difficulty >= q->difficulty) {
            current = current->next;
        }
        
        new_node->next = current->next;
        current->next = new_node;
    }
}