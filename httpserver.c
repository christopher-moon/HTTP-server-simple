
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

#include "asgn2_helper_funcs.h"

//request struct
//-----------------------------------------------------------------------------------------------------------------------------
typedef struct Request {

    //general request specifications
    char *method;

    char *uri;

    char *version;

    //header fields
    char *key;

    char *value;

    //actual message text
    char *message;

    //for logical use
    int length;

    int leftover;

} Request;

//generic error function
//-----------------------------------------------------------------------------------------------------------------------------
void errormes(int length, int sd, int num) {

    //??? check this out later
    if (num == 200) {

        //dprintf(sd, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", size);

        dprintf(sd, "HTTP/1.1 %d OK\r\nContent-Length: %d\r\n\r\nOK\n", num, length);
    }

    if (num == 201) {

        dprintf(sd, "HTTP/1.1 %d Created\r\nContent-Length: %d\r\n\r\nCreated\n", num, length);
    }

    if (num == 400) {

        dprintf(
            sd, "HTTP/1.1 %d Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", num, length);
    }

    if (num == 403) {

        dprintf(sd, "HTTP/1.1 %d Forbidden\r\nContent-Length: %d\r\n\r\nForbidden\n", num, length);
    }

    if (num == 404) {

        dprintf(sd, "HTTP/1.1 %d Not Found\r\nContent-Length: %d\r\n\r\nNot Found\n", num, length);
    }

    if (num == 500) {

        dprintf(sd,
            "HTTP/1.1 %d Internal Server Error\r\nContent-Length: %d\r\n\r\nInternal Server "
            "Error\n",
            num, length);
    }

    if (num == 501) {

        dprintf(sd, "HTTP/1.1 %d Not Implemented\r\nContent-Length: %d\r\n\r\nNot Implemented\n",
            num, length);
    }

    if (num == 505) {

        dprintf(sd,
            "HTTP/1.1 %d Version Not Supported\r\nContent-Length: %d\r\n\r\nVersion Not "
            "Supported\n",
            num, length);
    }
}

//fill out request struct from a given buffer
//-----------------------------------------------------------------------------------------------------------------------------
int fillReqInfo(Request *req, char *buffer, int sd, ssize_t br) {

    //regex variabe
    regex_t preg;

    int bytesread = 0;

    //three fields
    //1) method = get/put (8 characters, a-z)
    //2) uri = 2-64 characters a-z and 0-9
    //3) version = HTTP/#.#

    //specify to look for the method, uri, version
    int r = regcomp(
        &preg, "^([a-zA-Z]{1,8}) /([a-zA-Z0-9.-]{1,63}) (HTTP/[0-9]\\.[0-9])\r\n", REG_EXTENDED);

    //array to hold matches (total, method, uri, version)
    regmatch_t pmatch[4];

    //search for matches in buffer
    r = regexec(&preg, buffer, 4, pmatch, 0);

    //if successful
    if (r == 0) {

        //get data from pmatch, and put into struct fields

        //get method
        req->method = buffer + pmatch[1].rm_so; //start here

        req->method[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0'; //cutoff here

        //get uri
        req->uri = buffer + pmatch[2].rm_so;

        req->uri[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';

        //get version
        req->version = buffer + pmatch[3].rm_so;

        req->version[pmatch[3].rm_eo - pmatch[3].rm_so] = '\0';

        //update bytesread to reflect reading the method, uri, and version
        bytesread = pmatch[3].rm_eo + 2;

        //skip buffer to content length
        //+2 for the \r\n
        buffer += pmatch[3].rm_eo + 2;

        //search bufffer for headers (key and value)
        //THIS IS SUPER IMPORTANT: THERE CAN BE MULTIPLE HEADERS SO USE A WHILE LOOP TO GO THRU ALL OF THEM

        //inital search
        r = regcomp(&preg, "([a-zA-Z0-9.-]{1,128}):([ -~]{1,128})\r\n", REG_EXTENDED);

        r = regexec(&preg, buffer, 3, pmatch, 0);

        //inital values for content length and value, returned in case of error
        req->length = 0;

        int value = 0;

        //while there are still headers left over
        while (buffer[0] != '\r') {

            //if regexec fails
            if (r != 0) {

                //error
                regfree(&preg);

                //fprintf(stderr, "r err\n");
                errormes(12, sd, 400);

                return 1;
            }

            //get key
            req->key = buffer + pmatch[1].rm_so; //start here

            req->key[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0'; //cutoff here

            //get value
            req->value = buffer + pmatch[2].rm_so; //start here

            req->value[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0'; //cutoff here

            //if we find a "content length" key
            if (strcmp(req->key, "Content-Length") == 0) {

                //if we havent already done this
                if (value == 0) {

                    //get the corresponding val
                    //convert val to int
                    value = atoi(req->value);

                    //if val is bad
                    if (value == 0) {

                        //error
                        errormes(0, sd, 400);

                        return 1;
                    }

                    //update content length
                    req->length = value;
                }
            }

            //else, continue iterating thru headers
            //+2 for the \r\n
            buffer += pmatch[2].rm_eo + 2;

            //increment bytesread
            bytesread += pmatch[2].rm_eo + 2;

            //move to next header
            r = regexec(&preg, buffer, 3, pmatch, 0);
        }

        //once we are done with all headers, get message
        bytesread += 2;

        req->message = buffer + 2;

        req->leftover = br - bytesread;

        //free
        regfree(&preg);

        return 0;
    }

    //error, unsuccessful regexec
    regfree(&preg);

    errormes(12, sd, 400);

    return 1;
}

//get
//-----------------------------------------------------------------------------------------------------------------------------
int get(Request *req, int sd) {

    //make sure theres no message/content length for a get request
    if (req->length > 0) {

        //error
        errormes(12, sd, 400);

        return 1;
    }

    //attempt to open file specified by uri
    int fd = open(req->uri, O_RDONLY);

    //if unable to open file, figure out specific error
    if (fd == -1) {

        //not allowed
        if (errno == EACCES) {

            errormes(10, sd, 403);

            return 1;

            //not found
        } else if (errno == ENOENT) {

            errormes(10, sd, 404);

            return 1;

            //anything else
        } else {

            errormes(22, sd, 500);

            return 1;
        }
    }

    //file stat create (this is for checking the size of the file and the type)
    struct stat fs;

    //if unable to fstat
    if (fstat(fd, &fs) == -1) {

        close(fd);
        //internal error
        errormes(22, sd, 500);

        return 1;
    }

    //get size
    int size = fs.st_size;

    //check if not a regular file
    if (S_ISREG(fs.st_mode) == 0) {

        //close and error
        close(fd);

        errormes(10, sd, 403);

        return 1;
    }

    //OK message (note no "OK\n" at the end)
    dprintf(sd, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", size);

    //errormes(size, sd, 200);

    //pass bytes ("getting")
    int passed = pass_n_bytes(fd, sd, size);

    //if getting failed
    if (passed == -1) {

        //error
        errormes(22, sd, 500);

        return 1;
    }

    //close and return success
    close(fd);

    return 0;
}

//put
//-----------------------------------------------------------------------------------------------------------------------------
int put(Request *req, int sd) {

    //needs to include a valid content length
    if (req->length == 0) {

        //error if no content
        errormes(12, sd, 400);

        return 1;
    }

    //check if file exists

    //open
    int fd = open(req->uri, O_WRONLY | O_TRUNC);

    //handle any errors
    if (fd == -1) {

        //not allowed
        if (errno == EACCES) {

            errormes(10, sd, 403);

            return 1;

            //anything else besides does not exist
        } else if (errno != ENOENT) {

            errormes(22, sd, 500);

            return 1;
        }
    }

    //file stat create (this is for checking the size of the file (0 == doesnt exist) and the type)
    struct stat fs;

    //fstat
    int x = fstat(fd, &fs);

    //use this to track if the file exists or not
    bool exists;

    //if unable to fstat
    if (x == -1) {

        if (errno == EACCES) {

            close(fd);

            errormes(10, sd, 403);

            return 1;

        } else {

            close(fd);

            exists = false;
        }

    } else {

        exists = true;

        //check if not a regular file
        if (S_ISREG(fs.st_mode) == 0) {

            //close and error
            close(fd);

            errormes(10, sd, 403);

            return 1;
        }

        close(fd);
    }

    //open or create for writing
    fd = open(req->uri, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    //write to file, or "put"
    int written = write_n_bytes(fd, req->message, req->leftover);

    //if there is an error writing bytes
    if (written == -1) {

        errormes(22, sd, 500);

        return 1;
    }

    int bytes = req->length - req->leftover;

    written = pass_n_bytes(sd, fd, bytes);

    if (written == -1) {

        errormes(22, sd, 500);
        return 1;
    }

    //handle return message
    if (exists == true) {

        //just written to
        errormes(3, sd, 200);

    } else {

        //created file and written to
        errormes(8, sd, 201);
    }

    close(fd);
    return 0;
}

//main server
int main(int argc, char *argv[]) {

    //create socket
    Listener_Socket socket;

    //convert port argument from a string to an int
    int port = atoi(argv[1]);

    //if port is invalid, error
    if (port == 0) {

        fprintf(stderr, "Invalid Port\n");

        return 1;
    }

    //attempt to bind port and socket
    int bind = listener_init(&socket, port);

    //if failure binding, error
    if (bind == -1) {

        fprintf(stderr, "Invalid Port\n");

        return 1;
    }

    //create buffer to hold requests
    char buffer[2048];

    //initialize buffer
    memset(buffer, 0, 2048);

    //forever
    while (true) {

        //accept socket
        int sd = listener_accept(&socket);

        //if error accepting socket
        if (sd == -1) {

            fprintf(stderr, "Unable to Establish Connection\n");

            return 1;
        }

        //create new request struct instance
        Request r;

        //read request (including message body if applicable) into buffer, put total read bytes into rb
        ssize_t rb = read_until(sd, buffer, 2048, "\r\n\r\n");

        //if unable to read request, error
        if (rb == -1) {

            dprintf(sd, "HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", 12);

            return 1;
        }

        //read data from buffer, and put into request struct
        int x = fillReqInfo(&r, buffer, sd, rb);

        //if able to get data, do the request (either GET or PUT)
        if (x != 1) {

            //if bad version (anything not 1.1), error
            if (strcmp(r.version, "HTTP/1.1") != 0) {

                dprintf(sd,
                    "HTTP/1.1 505 Version Not Supported\r\nContent-Length: %d\r\n\r\nVersion Not "
                    "Supported\n",
                    22);
                return 1;
            }

            //get
            if (strcmp(r.method, "GET") == 0) {

                get(&r, sd);

                //put
            } else if (strcmp(r.method, "PUT") == 0) {

                put(&r, sd);

                //something else, error
            } else {

                errormes(16, sd, 501);

                return 1;
            }
        }

        //close socket descriptor and reset buffer
        memset(buffer, 0, 2048);

        close(sd);
    }

    return 0;
}
