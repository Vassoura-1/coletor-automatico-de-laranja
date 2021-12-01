#include "status_server.h"
#include "fallback_index.h"

#include "variaveis_globais.h"

httpd_handle_t control_httpd = NULL;

// Serve a página inicial da aplicação usando SPIFFS
static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    File index_file = SPIFFS.open("/index.html.gz");
    size_t file_size = index_file.size();

    if(file_size > 0){
        char * buffer = (char *) malloc(file_size);

        while(index_file.available()){
            index_file.readBytes(buffer, file_size);
        }

        esp_err_t flag = httpd_resp_send(req, buffer, file_size);

        index_file.close();
        free(buffer);
        
        return flag;
    } else {
        index_file.close();
        return httpd_resp_send(req, (const char *)fallback_html_gz, fallback_html_gz_len);
    }
}

// processa a requisição solicitando os status da aplicação
static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];

    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"state\":%d,", current_state);

    p+=sprintf(p, "\"d1\":%u,", leituras.d1);
    p+=sprintf(p, "\"d2\":%u,", leituras.d2);
    p+=sprintf(p, "\"cam_read\":%d,", leituras.cam);
    p+=sprintf(p, "\"start\":%d,", leituras.start);
    p+=sprintf(p, "\"u1\":%u,", leituras.u1);
    p+=sprintf(p, "\"u2\":%u,", leituras.u2);
    p+=sprintf(p, "\"encpos\":%d,", leituras.encpos);
    p+=sprintf(p, "\"stop11\":%d,", leituras.stop11);
    p+=sprintf(p, "\"stop12\":%d,", leituras.stop12);
    p+=sprintf(p, "\"stop21\":%d,", leituras.stop21);
    p+=sprintf(p, "\"stop22\":%d,", leituras.stop22);
    
    p+=sprintf(p, "\"sole11\":%d,", saidas.sole11);
    p+=sprintf(p, "\"sole12\":%d,", saidas.sole12);
    p+=sprintf(p, "\"sole21\":%d,", saidas.sole21);
    p+=sprintf(p, "\"sole22\":%d,", saidas.sole22);
    p+=sprintf(p, "\"cam_en\":%d,", saidas.cam);
    p+=sprintf(p, "\"motor11\":%d,", saidas.motor11);
    p+=sprintf(p, "\"motor12\":%d,", saidas.motor12);
    p+=sprintf(p, "\"motor1_vel\":%u,", saidas.motor1_vel);
    p+=sprintf(p, "\"reset_encoder\":%u,", saidas.reset_encoder);

    p+=sprintf(p, "\"T1\":%llu,", ext_T1);
    p+=sprintf(p, "\"T2\":%llu,", ext_T2);

    p+=sprintf(p, "\"T1_isr_c\":%d,", ext_T1_icnt);
    p+=sprintf(p, "\"T2_isr_c\":%d,", ext_T2_icnt);

    
    p+=sprintf(p, "\"free_Heap\":%d", ESP.getFreeHeap());
    
    *p++ = '}';
    *p++ = 0;

    log_v("%s",json_response);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

// procressa requisições para a mudança de parâmetros da câmera
static esp_err_t cmd_handler(httpd_req_t *req){
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) 
            {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    int res = 0;

    if(!strcmp(variable, "estado")) current_state = (state_t) val; // setar estado
    else if(!strcmp(variable, "d1")) leituras.d1 = (uint16_t) val; // setar leitura d1
    else if(!strcmp(variable, "d2")) leituras.d2 = (uint16_t) val; // setar leitura d2
    else if(!strcmp(variable, "cam_en")) leituras.cam = (bool) val; // setar leitura cam
    else if(!strcmp(variable, "start")) leituras.start = (bool) val; // setar leitura start
    else if(!strcmp(variable, "u1")) leituras.u1 = (uint) val; // setar leitura u1
    else if(!strcmp(variable, "u2")) leituras.u2 = (uint) val; // setar leitura u2
    else if(!strcmp(variable, "stop11")) leituras.stop11 = (bool) val; // setar leitura u1
    else if(!strcmp(variable, "stop12")) leituras.stop12 = (bool) val; // setar leitura u2
    else if(!strcmp(variable, "stop21")) leituras.stop21 = (bool) val; // setar leitura u1
    else if(!strcmp(variable, "stop22")) leituras.stop22 = (bool) val; // setar leitura u2
    else if(!strcmp(variable, "encpos")) leituras.encpos = (int) val; // setar leitura u2
    else {
        res = -1;
    }

    if(res){
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

// Inicializa os servidores HTTP e associa cada handler a sua URL
void startStatusServer(){
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    #ifndef USE_SENSORS
        httpd_uri_t cmd_uri = {
            .uri       = "/control",
            .method    = HTTP_GET,
            .handler   = cmd_handler,
            .user_ctx  = NULL
        };
    #endif

    // ra_filter_init(&ra_filter, 20);
    
    log_i("Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&control_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(control_httpd, &index_uri);
        httpd_register_uri_handler(control_httpd, &status_uri);
        #ifndef USE_SENSORS
            httpd_register_uri_handler(control_httpd, &cmd_uri);
        #endif 
    } else {
        log_e("Failed to start web server");
    }
}

// Encerra os servidores
void stopStatusServer(){
    if(control_httpd != NULL){
        if(httpd_stop(control_httpd) == ESP_OK)
            log_i("Servidor web encerrado");
    }
}