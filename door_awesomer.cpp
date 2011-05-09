#include <WProgram.h>
#include <Ethernet.h>
#include <NewSoftSerial.h>
#include <avr/pgmspace.h>
#include "content.h"

byte mac[] = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0x01 };
byte ip[] = { 10, 0, 0, 7 };
byte gateway[] = { 10, 0, 0, 1 };
byte subnet[] = { 255, 255, 255, 0 };

#define IMG_REQUEST "GET /title.png HTTP"
#define POST_REQUEST "POST /action HTTP"

#define BUFFER_SIZE 128
char buffer[BUFFER_SIZE];
char action_buffer[BUFFER_SIZE];

char redirect_header[] = "HTTP/1.0 302 Found\nLocation: /\nServer: arduino\nConnection: close\n\n";

char header[] = "HTTP/1.0 200 OK\nServer: arduino\nConnection: close\nContent-Type: text/html\n\n";

char img_header[] = "HTTP/1.0 200 OK\nServer: arduino\nConnection: close\nContent-Type: img/png\n\n";

#define ACTION_COUNT 5

char action0[] = "Front+Door";
char action1[] = "Exec+Door";
char action2[] = "Bike+Door";
char action3[] = "Kitchen";
char action4[] = "Shipping";
char *actions[] = { action0, action1, action2, action3, action4};

char cmd0[] = " O=1 O 2 P";
char cmd1[] = " O=2 O 1 P";
char cmd2[] = " O=1 O 1 P";
char cmd3[] = " O=1 O 3 P";
char cmd4[] = " O=1 O 4 P";
char *cmds[] = { cmd0, cmd1, cmd2, cmd3, cmd4 };

Server server(80);
Server log_server(2121);

#define TO_HARDWARE_SERIAL 1
#define TO_SOFT_SERIAL 2

#define compRx 6
#define compTx 7
NewSoftSerial soft_serial(compRx, compTx);

int read_http_request(Client client);
void send_progmem(Client client, char *content, int len);
void send_to_serial(char byte, short to, Client log_client);

extern "C" void __cxa_pure_virtual(void);
void __cxa_pure_virtual(void) {};

void setup() {
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();

  Serial.begin(9600);
  soft_serial.begin(9600);
}

void loop() {
  Client log_client = log_server.available();
  Client client = server.available();
  if (client) {

    int resp_idx = read_http_request(client);

    if (resp_idx == -1) {
      client.print(header);
      send_progmem(client, body, strlen_P(body));
    } else if (resp_idx == -2) {
      client.print(img_header);
      send_progmem(client, title_image, TITLE_IMG_LEN);
    } else {
      client.print(redirect_header);
      if (resp_idx >= 0 && resp_idx < ACTION_COUNT) {
        log_client.print("\n[AWESOMER][");
        log_client.print(actions[resp_idx]);
        log_client.print("]#");
        int len = strlen(cmds[resp_idx]);
        for (int i = 0; i < len; i++) {
          send_to_serial(cmds[resp_idx][i], TO_HARDWARE_SERIAL, log_client);
        }
        send_to_serial('\r', TO_HARDWARE_SERIAL, log_client);
      }
    }

    client.flush();
    client.stop();
  }

  if (soft_serial.available()) {
    log_client.print("\n[TO SECURITY]#");
    while (soft_serial.available()) {
      char got = soft_serial.read();
      send_to_serial(got, TO_HARDWARE_SERIAL, log_client);
    }
  }

  if (Serial.available()) {
    log_client.print("\n[FROM SECURITY]#");
    while (Serial.available()) {
      char got = Serial.read();
      send_to_serial(got, TO_SOFT_SERIAL, log_client);
    }
  }
}

void send_to_serial(char byte, short to, Client log_client) {
  if (byte < 32) {
    log_client.print("#{");
    log_client.print((int) byte);
    log_client.print("}");
  } else {
    log_client.print(byte);
  }
  if (to == TO_HARDWARE_SERIAL) {
    Serial.print(byte);
  } else {
    soft_serial.print(byte);
  }
}

void send_progmem(Client client, char *content, int len) {
  int index = 0;
  uint8_t buffer[BUFFER_SIZE];

  memset(buffer, 0, BUFFER_SIZE);

  while (index <= len) {
    int size = BUFFER_SIZE - 1;
    if (index + size > len) {
      size = len - index;
    }
    memcpy_P(buffer, content+index, size);
    client.write(buffer, size);

    index = index + BUFFER_SIZE-1;
  }
}

void read_next_header(char *buffer, int *read_count, Client client) {
  int index = 0;
  memset(buffer, 0, BUFFER_SIZE);

  buffer[0] = client.read();
  buffer[1] = client.read();
  index = 2;

  while (buffer[index-2] != '\r' && buffer[index-1] != '\n' && index < BUFFER_SIZE - 2) {
    char got = client.read();

    buffer[index] = got;
    index++;
  }
  *read_count = index;
}

int read_http_request(Client client) {
  int resp_idx = -1;

  if (client.connected() && client.available()) {
    int read_count = 0;
    read_next_header(buffer, &read_count, client);

    if (strncmp(buffer, POST_REQUEST, strlen(POST_REQUEST)) == 0) {
      int content_length = 0;

      while (client.available()) {
        read_count = 0;
        read_next_header(buffer, &read_count, client);

        if (read_count == 2) {
          break;
        }

        sscanf(buffer, "Content-Length: %d", &content_length);
      }

      memset(buffer, 0, BUFFER_SIZE);
      for (int i =0; i < content_length; i++) {
        buffer[i] = client.read();
      }

      if (sscanf(buffer, "open=%s", action_buffer)) {
        for (int i = 0; i < ACTION_COUNT; i++) {
          if (strcmp(action_buffer, actions[i]) == 0) {
            resp_idx = i;
            break;
          }
        }
      }
    } else if (sscanf(buffer, "GET /action?open=%s HTTP", action_buffer)) {
      for (int i = 0; i < ACTION_COUNT; i++) {
        if (strcmp(action_buffer, actions[i]) == 0) {
          resp_idx = i;
          break;
        }
      }
    } else if (strncmp(buffer, IMG_REQUEST, strlen(IMG_REQUEST)) == 0) {
      resp_idx = -2;
    }
  }

  return resp_idx;
}

int main(void) {
  init();
  setup();

  for (;;) {
    loop();
  }

  return 0;
}
