#include "webservice.h"
#include "packages/json/json.h"
#include "packages/postgresql/postgresql.h"

DUDA_REGISTER("PostgreSQL package usage demonstration", "postgresql_demo");

static inline void print_header(duda_request_t *dr)
{
    response->printf(dr, "\n\
<!DOCTYPE html>\n\
<html lang=\"en\">\n\
<head>\n\
  <meta charset=\"UTF-8\">\n\
  <title>PostgreSQL Package Demo</title>\n\
  <link rel=\"stylesheet\" href=\"//netdna.bootstrapcdn.com/bootstrap/3.0.0/css/bootstrap.min.css\">\n\
  <style type=\"text/css\">\n\
    body {\n\
      position: relative;\n\
      padding-top: 80px;\n\
    }\n\
  </style>\n\
</head>\n\
<body>\n\
  <div class=\"navbar navbar-inverse navbar-fixed-top\">\n\
    <div class=\"container\">\n\
      <a href=\"#\" class=\"navbar-brand\">PostgreSQL Package</a>\n\
      <ul class=\"nav navbar-nav\">\n\
        <li id=\"home\">\n\
          <a href=\"/postgresql_demo/postgresql/home/\">Home</a>\n\
        </li>\n\
        <li id=\"dashboard\">\n\
          <a href=\"/postgresql_demo/postgresql/dashboard/\">Dashboard</a>\n\
        </li>\n\
      </ul>\n\
    </div>\n\
  </div>");
}

static inline void print_footer(duda_request_t *dr, const char *js)
{
    response->printf(dr, "\n\
  <script src=\"//ajax.googleapis.com/ajax/libs/jquery/1.10.1/jquery.min.js\"></script>\n\
  <script src=\"//netdna.bootstrapcdn.com/bootstrap/3.0.0/js/bootstrap.min.js\"></script>");
    if (js) {
        response->printf(dr, js);
    }
    response->printf(dr, "\n\
</body>\n\
</html>");
}

void cb_on_connect(postgresql_conn_t *conn, int status, duda_request_t *dr)
{
    if (status != POSTGRESQL_OK) {
        response->printf(dr, "Can't connect to PostgreSQL server\n");
        response->cont(dr);
        response->end(dr, NULL);
    }
}

void cb_on_disconnect(postgresql_conn_t *conn, int status, duda_request_t *dr)
{
    if (status != POSTGRESQL_OK) {
        response->printf(dr, "Disconnect due to some errors\n");
    }
    response->cont(dr);
    response->end(dr, NULL);
}

void cb_ls_row_simple(void *data, postgresql_query_t *query, int n_fields,
                      char **fields, char **values, duda_request_t *dr)
{
    json_t *array = data;
    json_t *item;
    (void) query;
    (void) fields;
    (void) dr;
    if (n_fields == 1) {
        item = json->create_string(values[0]);
        json->add_to_array(array, item);
    }
}

void cb_ls_result(void *data, postgresql_query_t *query, int n_fields,
                  char **fields, duda_request_t *dr)
{
    json_t *root = data;
    json_t *array;
    json_t *item;
    (void) query;
    (void) dr;
    unsigned long i;
    array = json->create_array();
    for (i = 0; i < n_fields; ++i) {
        item = json->create_string(fields[i]);
        json->add_to_array(array, item);
    }
    json->add_to_object(root, "fields", array);
    array = json->create_array();
    json->add_to_object(root, "rows", array);
}

void cb_ls_row(void *data, postgresql_query_t *query, int n_fields, char **fields,
               char **values, duda_request_t *dr)
{
    json_t *root = data;
    json_t *array = json->get_object_item(root, "rows");
    json_t *item = json->create_object();
    json_t *column;
    (void) query;
    (void) dr;
    unsigned long i;
    for (i = 0; i < n_fields; ++i) {
        if (values[i] == NULL) {
            column = json->create_null();
        } else {
            column = json->create_string(values[i]);
        }
        json->add_to_object(item, fields[i], column);
    }
    json->add_to_array(array, item);
}

void cb_ls_end(void *data, postgresql_query_t *query, duda_request_t *dr)
{
    (void) query;
    json_t *array = data;
    char *encoded_str = json->print_gc(dr, array);
    response->printf(dr, "%s", encoded_str);
}

void cb_list_databases(duda_request_t *dr)
{
    json_t *db_array = json->create_array();
    response->http_status(dr, 200);
    response->http_header_n(dr, "Content-Type: application/json", 30);
    response->wait(dr);

    postgresql_conn_t *conn = postgresql->connect_uri(dr, cb_on_connect,
                                                      "user=postgres password=foo");
    if (!conn) {
        response->printf(dr, "No Connection Available\n");
        response->cont(dr);
        response->end(dr, NULL);
    }

    postgresql->query(conn, "SELECT datname FROM pg_database WHERE datistemplate = false",
                      NULL, cb_ls_row_simple, cb_ls_end, (void *) db_array);
    postgresql->disconnect(conn, cb_on_disconnect);
}

void cb_list_tables(duda_request_t *dr)
{
    json_t *tb_array = json->create_array();
    char *db = param->get(dr, 0);
    char uri[128];

    response->http_status(dr, 200);
    response->http_header_n(dr, "Content-Type: application/json", 30);
    response->wait(dr);

    sprintf(uri, "user=postgres password=foo dbname=%s", db);
    postgresql_conn_t *conn = postgresql->connect_uri(dr, cb_on_connect, uri);
    if (!conn) {
        response->printf(dr, "No Connection Available\n");
        response->cont(dr);
        response->end(dr, NULL);
    }

    postgresql->query(conn, "SELECT table_name FROM information_schema.tables \
                      WHERE table_schema = 'public'", NULL, cb_ls_row_simple,
                      cb_ls_end, (void *)tb_array);
    postgresql->disconnect(conn, cb_on_disconnect);
}

void cb_row_nums(duda_request_t *dr)
{
    json_t *row_nums = json->create_array();
    char *db = param->get(dr, 0);
    char *table = param->get(dr, 1);
    char uri[128];
    char count_query[128];

    response->http_status(dr, 200);
    response->http_header_n(dr, "Content-Type: application/json", 30);
    response->wait(dr);

    sprintf(uri, "user=postgres password=foo dbname=%s", db);
    postgresql_conn_t *conn = postgresql->connect_uri(dr, cb_on_connect, uri);
    if (!conn) {
        response->printf(dr, "No Connection Available\n");
        response->cont(dr);
        response->end(dr, NULL);
    }

    sprintf(count_query, "SELECT COUNT(*) FROM %s", table);
    postgresql->query(conn, count_query, NULL, cb_ls_row_simple, cb_ls_end, (void *)row_nums);
    postgresql->disconnect(conn, cb_on_disconnect);
}

void cb_list_rows(duda_request_t *dr)
{
    json_t *row_root = json->create_object();
    char *db    = param->get(dr, 0);
    char *table = param->get(dr, 1);
    long page;
    param->get_number(dr, 2, &page);
    char uri[128];
    char select_query[128];
    int page_size = 20;

    response->http_status(dr, 200);
    response->http_header_n(dr, "Content-Type: application/json", 30);
    response->wait(dr);

    sprintf(uri, "user=postgres password=foo dbname=%s", db);
    postgresql_conn_t *conn = postgresql->connect_uri(dr, cb_on_connect, uri);
    if (!conn) {
        response->printf(dr, "No Connection Available\n");
        response->cont(dr);
        response->end(dr, NULL);
    }

    sprintf(select_query, "SELECT * FROM %s LIMIT %d OFFSET %ld", table, page_size,
            (page - 1) * page_size);
    postgresql->query(conn, select_query, cb_ls_result, cb_ls_row, cb_ls_end, (void *) row_root);
    postgresql->disconnect(conn, cb_on_disconnect);
}

void cb_home_page(duda_request_t* dr)
{
    response->http_status(dr, 200);
    response->http_header_n(dr, "Content-Type: text/html", 23);
    print_header(dr);
    response->printf(dr, "\n\
  <div class=\"container\">\n\
    <div class=\"jumbotron\">\n\
      <h1>Non-blocking PostgreSQL Access</h1>\n\
      <p>\n\
        This is a Duda web service that demonstrates the usage of Duda PostgreSQL package.\n\
        If you're interested in the package itself, please refer to\n\
        <a href=\"https://github.com/swpd/duda_postgresql\">this</a>.\n\
      </p>\n\
      <p><a class=\"btn btn-primary btn-lg\" href=\"/postgresql_demo/postgresql/dashboard/\">Get Started »</a></p>\n\
    </div>\n\
  </div>");

    char *js = "\n\
  <script>\n\
    $(document).ready(function() {\n\
      $('#home').addClass('active');\n\
    });\n\
  </script>";
    print_footer(dr, js);
    response->end(dr, NULL);
}

void cb_dashboard(duda_request_t *dr)
{
    response->http_status(dr, 200);
    response->http_header_n(dr, "content-type: text/html", 23);
    print_header(dr);
    response->printf(dr, "\n\
  <div class=\"container\">\n\
    <div class=\"panel panel-default\">\n\
      <div class=\"panel-heading\">\n\
        <h4 class=\"panel-title\">Dashboard</h4>\n\
      </div>\n\
      <div class=\"panel-body\">\n\
        <div class=\"row\">\n\
          <div class=\"col-lg-4 col-lg-offset-2\">\n\
            <form class=\"form-inline\">\n\
              <label>choose a database:</label>\n\
              <div class=\"btn-group\">\n\
                <button id=\"current-db\" type=\"button\" class=\"btn btn-default\"></button>\n\
                <button type=\"button\" class=\"btn btn-default dropdown-toggle\" data-toggle=\"dropdown\">\n\
                  <span class=\"caret\"></span>\n\
                </button>\n\
                <ul id=\"db-list\" class=\"dropdown-menu\">\n\
                </ul>\n\
              </div>\n\
            </form>\n\
          </div>\n\
          <div class=\"col-lg-4\">\n\
            <form class=\"form-inline\">\n\
              <label>choose a table:</label>\n\
              <div class=\"btn-group\">\n\
                <button id=\"current-tb\" type=\"button\" class=\"btn btn-default\"></button>\n\
                <button type=\"button\" class=\"btn btn-default dropdown-toggle\" data-toggle=\"dropdown\">\n\
                  <span class=\"caret\"></span>\n\
                </button>\n\
                <ul id=\"tb-list\" class=\"dropdown-menu\">\n\
                </ul>\n\
              </div>\n\
            </form>\n\
          </div>\n\
        </div>\n\
      </div>\n\
    </div>\n\
    <div class=\"row\">\n\
      <div class=\"col-lg-12\">\n\
        <span style=\"display: none\" id=\"total_page\"></span>\n\
        <span style=\"display: none\" id=\"current_page\"></span>\n\
        <ul id=\"pager\" class=\"pager\">\n\
        </ul>\n\
      </div>\n\
    </div>\n\
    <table id=\"rows\" class=\"table table-bordered\">\n\
    </table>\n\
  </div>");

    char *js = "\n\
  <script>\n\
    $(document).ready(function() {\n\
      $('#dashboard').addClass('active');\n\
      $.ajax({\n\
        url: '/postgresql_demo/postgresql/list-databases/',\n\
        success: function(data) {\n\
          for (var i in data) {\n\
            $('#db-list').append('<li><a>' + data[i] + '</a></li>');\n\
          }\n\
          $('#db-list li').click(function() {\n\
            var db = $(this).text();\n\
            $('#current-db').text(db);\n\
            $.ajax({\n\
              url: '/postgresql_demo/postgresql/list-tables/' + db + '/',\n\
              success: function(data) {\n\
                $('#tb-list').children().remove();\n\
                for (var i in data) {\n\
                  $('#tb-list').append('<li><a>' + data[i] + '</a></li>');\n\
                }\n\
                $('#tb-list li').click(function() {\n\
                  var db = $('#current-db').text();\n\
                  var tb = $(this).text();\n\
                  $('#current-tb').text(tb);\n\
                  $.ajax({\n\
                    url: '/postgresql_demo/postgresql/row-nums/' + db + '/' + tb + '/',\n\
                    success: function(data) {\n\
                      $('#pager').children().remove();\n\
                      $('#pager').append('<li tag=\"1\"><a>&laquo;</a></li>');\n\
                      $('#pager').append('<li><a>prev</a></li>');\n\
                      var page_nums = Math.ceil(parseInt(data[0]) / 20);\n\
                      $('#total_page').text(page_nums);\n\
                      var end = page_nums <= 9 ? page_nums : 9;\n\
                      for (var i = 1; i <= end; ++i) {\n\
                        $('#pager').append('<li tag=\"' + i + '\"><a>' + i + '</a></li>');\n\
                      }\n\
                      $('#pager').append('<li><a>next</a></li>');\n\
                      $('#pager').append('<li tag=\"' + page_nums + '\"><a>&raquo;</a></li>');\n\
                      function pager_cb() {\n\
                        var selected = $(this).text();\n\
                        var total_page = parseInt($('#total_page').text());\n\
                        var page;\n\
                        if (selected === 'prev') {\n\
                          var current_page = parseInt($('#current_page').text());\n\
                          page = current_page - 1 >= 1 ? current_page - 1 : 1;\n\
                        } else if (selected === 'next'){\n\
                          var current_page = parseInt($('#current_page').text());\n\
                          page = current_page + 1 <= total_page ? current_page + 1 : total_page;\n\
                        } else {\n\
                          page = parseInt($(this).attr('tag'));\n\
                        }\n\
                        $('#current_page').text(page);\n\
                        var start = page - 4 >= 1 ? page - 4 : 1;\n\
                        var end = page + 4 <= total_page ? page + 4 : total_page\n\
                        if (start == 1) {\n\
                          end = total_page <= 9 ? total_page : 9;\n\
                        } else if (end == total_page) {\n\
                          start = total_page > 9 ? total_page - 8 : 1;\n\
                        }\n\
                        $('#pager').children().remove();\n\
                        $('#pager').append('<li tag=\"1\"><a>&laquo;</a></li>');\n\
                        $('#pager').append('<li><a>prev</a></li>');\n\
                        for (var i = start; i <= end; ++i) {\n\
                          $('#pager').append('<li tag=\"' + i + '\"><a>' + i + '</a></li>');\n\
                        }\n\
                        $('#pager').append('<li><a>next</a></li>');\n\
                        $('#pager').append('<li tag=\"' + total_page + '\"><a>&raquo;</a></li>');\n\
                        $('#pager li[tag=\"' + page + '\"]').addClass('disabled');\n\
                        $('#pager li').click(pager_cb);\n\
                        $.ajax({\n\
                          url: '/postgresql_demo/postgresql/list-rows/' + db + '/' + tb + '/' + page + '/',\n\
                          success: function(data) {\n\
                            $('#rows').children().remove();\n\
                            $('#rows').append('<tr>');\n\
                            for (var i in data.fields) {\n\
                              $('#rows tr').last().append('<td>' + data.fields[i] + '</td>');\n\
                            }\n\
                            $('#rows').append('</tr>');\n\
                            for (var ri in data.rows) {\n\
                              $('#rows').append('<tr>');\n\
                              for (var fi in data.fields) {\n\
                                $('#rows tr').last().append('<td>' + data.rows[ri][data.fields[fi]] + '</td>');\n\
                              }\n\
                              $('#rows').append('</tr>');\n\
                            }\n\
                          }\n\
                        });\n\
                      }\n\
                      $('#pager li').click(pager_cb);\n\
                      $('#pager li').first().click();\n\
                    }\n\
                  });\n\
                });\n\
                $('#tb-list li').first().click();\n\
              }\n\
            });\n\
          });\n\
          $('#db-list li').first().click();\n\
        }\n\
      });\n\
    });\n\
  </script>";
    print_footer(dr, js);
    response->end(dr, NULL);
}

int duda_main()
{
    duda_load_package(postgresql, "postgresql");
    duda_load_package(json, "json");

    duda_interface_t *if_system;
    duda_method_t *method;
    duda_param_t *params;

    if_system = map->interface_new("postgresql");

    method = map->method_new("home", "cb_home_page", 0);
    map->interface_add_method(method, if_system);

    method = map->method_new("dashboard", "cb_dashboard", 0);
    map->interface_add_method(method, if_system);

    method = map->method_new("list-databases", "cb_list_databases", 0);
    map->interface_add_method(method, if_system);

    method = map->method_new("list-tables", "cb_list_tables", 1);
    params = map->param_new("database", 64);
    map->method_add_param(params, method);
    map->interface_add_method(method, if_system);

    method = map->method_new("row-nums", "cb_row_nums", 2);
    params = map->param_new("database", 64);
    map->method_add_param(params, method);
    params = map->param_new("table", 64);
    map->method_add_param(params, method);
    map->interface_add_method(method, if_system);

    method = map->method_new("list-rows", "cb_list_rows", 3);
    params = map->param_new("database", 64);
    map->method_add_param(params, method);
    params = map->param_new("table", 64);
    map->method_add_param(params, method);
    params = map->param_new("page", 8);
    map->method_add_param(params, method);
    map->interface_add_method(method, if_system);

    duda_service_add_interface(if_system);
    return 0;
}
