#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "updater.h"

#define UPDATE_CHECK_URL "https://api.github.com/repos/kappaborg/Task-Manager/releases/latest"
#define UPDATE_CONFIG_FILE "update_config.json"
#define CURRENT_VERSION "1.2.0"
#define TOKEN_FILE ".github_token"

// Curl yazma işlemi için buffer yapısı
struct string_buffer {
    char *buffer;
    size_t size;
};

static bool auto_update_enabled = true;
static CURL *curl_handle = NULL;
static char github_token[256] = "";

// GitHub token'ı oku
static bool load_github_token(void) {
    FILE *token_file = fopen(TOKEN_FILE, "r");
    if (!token_file) {
        return false;
    }
    
    if (fgets(github_token, sizeof(github_token), token_file) == NULL) {
        fclose(token_file);
        return false;
    }
    
    // Yeni satırı kaldır
    github_token[strcspn(github_token, "\r\n")] = 0;
    
    fclose(token_file);
    return strlen(github_token) > 0;
}

// CURL yazma callback'i
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct string_buffer *mem = (struct string_buffer *)userp;
    
    char *ptr = realloc(mem->buffer, mem->size + realsize + 1);
    if (!ptr) return 0;
    
    mem->buffer = ptr;
    memcpy(&(mem->buffer[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->buffer[mem->size] = 0;
    
    return realsize;
}

// Güncelleme sistemini başlat
bool init_updater(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = curl_easy_init();
    
    if (!curl_handle) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return false;
    }
    
    // GitHub token'ı yükle
    load_github_token();
    
    // Yapılandırmayı yükle
    FILE *config = fopen(UPDATE_CONFIG_FILE, "r");
    if (config) {
        json_object *json;
        char buffer[1024];
        size_t len = fread(buffer, 1, sizeof(buffer), config);
        fclose(config);
        
        if (len > 0) {
            buffer[len] = '\0';
            json = json_tokener_parse(buffer);
            if (json) {
                json_object *enabled;
                if (json_object_object_get_ex(json, "auto_update", &enabled)) {
                    auto_update_enabled = json_object_get_boolean(enabled);
                }
                json_object_put(json);
            }
        }
    }
    
    return true;
}

// Güncelleme sistemini temizle
void cleanup_updater(void) {
    if (curl_handle) {
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
    }
    curl_global_cleanup();
    
    // Yapılandırmayı kaydet
    FILE *config = fopen(UPDATE_CONFIG_FILE, "w");
    if (config) {
        json_object *json = json_object_new_object();
        json_object_object_add(json, "auto_update", json_object_new_boolean(auto_update_enabled));
        fprintf(config, "%s", json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
        fclose(config);
        json_object_put(json);
    }
}

// GitHub API için token ayarla
bool set_github_token(const char *token) {
    if (!token || strlen(token) == 0) {
        return false;
    }
    
    strncpy(github_token, token, sizeof(github_token) - 1);
    github_token[sizeof(github_token) - 1] = '\0';
    
    // Token'ı dosyaya kaydet
    FILE *token_file = fopen(TOKEN_FILE, "w");
    if (!token_file) {
        return false;
    }
    
    fprintf(token_file, "%s\n", github_token);
    fclose(token_file);
    
    return true;
}

// Güncellemeleri kontrol et
bool check_for_updates(update_info_t *info) {
    if (!curl_handle || !info) return false;
    
    struct string_buffer response = {0};
    struct curl_slist *headers = NULL;
    
    // User-Agent ve Authorization başlıkları ekle
    headers = curl_slist_append(headers, "User-Agent: TaskManager/1.0");
    
    if (strlen(github_token) > 0) {
        char auth_header[300];
        snprintf(auth_header, sizeof(auth_header), "Authorization: token %s", github_token);
        headers = curl_slist_append(headers, auth_header);
    }
    
    curl_easy_setopt(curl_handle, CURLOPT_URL, UPDATE_CHECK_URL);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    response.buffer = malloc(1);
    response.size = 0;
    
    CURLcode res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to check updates: %s\n", curl_easy_strerror(res));
        free(response.buffer);
        return false;
    }
    
    // HTTP yanıt kodunu kontrol et
    long http_code = 0;
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (http_code != 200) {
        fprintf(stderr, "GitHub API error (HTTP %ld): %s\n", http_code, response.buffer);
        free(response.buffer);
        return false;
    }
    
    // Parse JSON response
    json_object *json = json_tokener_parse(response.buffer);
    free(response.buffer);
    
    if (!json) return false;
    
    // Get version info
    json_object *tag_name, *assets, *body;
    if (json_object_object_get_ex(json, "tag_name", &tag_name) &&
        json_object_object_get_ex(json, "assets", &assets) &&
        json_object_object_get_ex(json, "body", &body)) {
        
        strncpy(info->current_version, CURRENT_VERSION, sizeof(info->current_version) - 1);
        strncpy(info->latest_version, json_object_get_string(tag_name), sizeof(info->latest_version) - 1);
        strncpy(info->changelog, json_object_get_string(body), sizeof(info->changelog) - 1);
        
        // Get download URL from assets
        json_object *asset = json_object_array_get_idx(assets, 0);
        if (asset) {
            json_object *browser_download_url;
            if (json_object_object_get_ex(asset, "browser_download_url", &browser_download_url)) {
                strncpy(info->download_url, json_object_get_string(browser_download_url), 
                        sizeof(info->download_url) - 1);
            }
        }
        
        // Compare versions
        info->update_available = strcmp(info->current_version, info->latest_version) < 0;
    }
    
    json_object_put(json);
    return true;
}

// Güncellemeyi indir
bool download_update(const char *url, const char *output_path) {
    if (!curl_handle || !url || !output_path) return false;
    
    FILE *fp = fopen(output_path, "wb");
    if (!fp) return false;
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: TaskManager/1.0");
    
    if (strlen(github_token) > 0) {
        char auth_header[300];
        snprintf(auth_header, sizeof(auth_header), "Authorization: token %s", github_token);
        headers = curl_slist_append(headers, auth_header);
    }
    
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headers);
    fclose(fp);
    
    return res == CURLE_OK;
}

// Güncellemeyi doğrula
bool verify_update_signature(const char *update_path) {
    // TODO: Implement signature verification
    return true;
}

// Güncellemeyi yükle
bool install_update(const char *update_path) {
    if (!update_path) return false;
    
    // Önce güncellemeyi doğrula
    if (!verify_update_signature(update_path)) {
        fprintf(stderr, "Update signature verification failed\n");
        return false;
    }
    
    // Platform'a göre kurulum yap
#ifdef __APPLE__
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "hdiutil attach \"%s\" && "
             "cp -R /Volumes/TaskManager/TaskManager.app /Applications/ && "
             "hdiutil detach /Volumes/TaskManager", update_path);
    return system(cmd) == 0;
#else
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "unzip -o \"%s\" -d \"%s\"", 
             update_path, getenv("APPDATA"));
    return system(cmd) == 0;
#endif
}

// Otomatik güncelleme ayarını değiştir
void set_auto_update(bool enabled) {
    auto_update_enabled = enabled;
}

// Otomatik güncelleme durumunu kontrol et
bool is_auto_update_enabled(void) {
    return auto_update_enabled;
} 