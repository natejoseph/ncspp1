#include "ClientProgram.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

// TODO: Error handling, getm

// Object Definitions
struct NBResponse
{
    int size;
    char *type;
    char *data;

    NBResponse(int &csoc)
    {
        char buf[5];
        buf[4] = '\0';
        recvLoop(csoc, buf, 4);
        this->size = atoi(buf);
        recvLoop(csoc, buf, 4);
        this->type = buf;
        if (size > 0)
        {
            char *data = new char[this->size];
            recvLoop(csoc, data, this->size);
            this->data = data;
            // cout << "Received data: " << this->data << endl;
        }
    }
};

// Function Definitions
int recvLoop(int csoc, char *data, const int size)
{
    int nleft, nread;
    char *ptr;

    ptr = data;
    nleft = size;
    while (nleft > 0)
    {
        nread = recv(csoc, ptr, nleft, 0);
        if (nread < 0)
        {
            cerr << "Read Error\n";
            break;
        }
        else if (nread == 0)
        {
            cerr << "Socket closed\n";
            break;
        }
        nleft -= nread;
        ptr += nread;
    }
    return size - nleft;
}

void errorMessagePrint() // TODO:
{
}

string messageEncode(string msg)
{
    string op = to_string(msg.size());
    while (op.size() < 4)
    {
        op = "0" + op;
    }
    op += "POST";
    op += msg;
    return op;
}

void printMessages(NBResponse *msg)
{
    cout << "Messages:\n";
    // data format: timestamp;name,timestamp,ttl,message)=msg-end=(

    string data = string(msg->data);
    string delimiter = ")=msg-end=(";
    size_t pos = 0;
    string token;
    while ((pos = data.find(delimiter)) != string::npos)
    {
        // format: timestamp;name,timestamp,ttl,message
        auto stt = data.find(";") + 1;
        cout << "From: " << data.substr(stt, data.find(",", stt) - stt) << endl;
        stt = data.find(",", stt) + 1;
        cout << "Timestamp: " << data.substr(stt, data.find(",", stt) - stt) << endl;
        stt = data.find(",", stt) + 1;
        cout << "TTL: " << data.substr(stt, data.find(",", stt) - stt) << endl;
        stt = data.find(",", stt) + 1;
        cout << "Message:\n"
             << data.substr(stt, pos - stt) << endl
             << endl;
        // token = data.substr(0, pos);
        //  cout << token << endl;
        data.erase(0, pos + delimiter.length());
    }
}

// Client Functions
void login(int &index, string &command, int &csoc, bool &loggedIn, string &username)
{
    if (loggedIn)
    {
        cout << "Already logged in\n";
        return;
    }
    char appmsg[40] = "0032LGIN";
    string user;
    for (int i = 8; i < 24; i++)
    {
        if (index < command.size() && command[index] != ' ')
        {
            appmsg[i] = command[index];
            user += command[index];
            index++;
        }
        else
        {
            appmsg[i] = ' ';
        }
    }
    if (command[index] != ' ')
    {
        cout << "Username error\n";
        return;
    }
    index++;
    bool password = false;
    for (int i = 24; i < 40; i++)
    {
        if (index < command.size() && command[index] != ' ')
        {
            appmsg[i] = command[index];
            index++;
            password = true;
        }
        else
        {
            appmsg[i] = ' ';
        }
    }
    if (index < command.size() && command[index] != ' ' || !password)
    {
        cout << "Password error\n";
        return;
    }
    send(csoc, appmsg, 40, 0);

    // Login response
    NBResponse *response = new NBResponse(csoc);
    if (strncmp(response->type, "GOOD", 4) == 0)
    {
        cout << "Login successful\n";
        loggedIn = true;
        username = user;
    }
    else if (strncmp(response->type, "ERRM", 4) == 0)
    {
        cout << "Login failed with error message\n";
    }
    else
    {
        cout << "Login error\n";
    }
}

void logout(int &csoc, bool &loggedIn, string &username)
{
    // Logout send
    if (!loggedIn)
    {
        cout << "Not logged in\n";
        return;
    }
    char appmsg[24] = "0016LOUT";
    int j = 0;
    for (int i = 8; i < 24; i++)
    {
        if (j < username.size())
        {
            appmsg[i] = username[j];
            j++;
        }
        else
        {
            appmsg[i] = ' ';
        }
    }
    send(csoc, appmsg, 24, 0);

    // Logout response
    NBResponse *response = new NBResponse(csoc);
    if (strncmp(response->type, "GOOD", 4) == 0)
    {
        cout << "Logout successful\n";
        loggedIn = false;
        username = "";
    }
    else if (strncmp(response->type, "ERRM", 4) == 0)
    {
        cout << "Logout failed with error message\n";
    }
    else
    {
        cout << "Logout error\n";
    }
}

void post(string &command, int &csoc, bool &loggedIn)
{
    if (!loggedIn)
    {
        cout << "Not logged in\n";
        return;
    }
    // Post send
    string msg = command.substr(5);
    string op = messageEncode(msg);
    char appmsg[op.length()];
    strcpy(appmsg, op.c_str());
    send(csoc, appmsg, op.length(), 0);

    // Post response
    NBResponse *response = new NBResponse(csoc);
    if (strncmp(response->type, "GOOD", 4) == 0)
    {
        cout << "Post successful\n";
    }
    else if (strncmp(response->type, "ERRM", 4) == 0)
    {
        cout << "Post failed with error message\n";
    }
    else
    {
        cout << "Post error\n";
    }
}

void getMessages(int &csoc, bool &loggedIn)
{
    if (!loggedIn)
    {
        cout << "Not logged in\n";
        return;
    }
    // Get Messages send
    char *appmsg = "0000GETM";
    send(csoc, appmsg, 8, 0);

    // Get Messages response
    NBResponse *response = new NBResponse(csoc);
    if (strncmp(response->type, "LIST", 4) == 0)
    {
        printMessages(response);
    }
    else if (strncmp(response->type, "ERRM", 4) == 0)
    {
        cout << "Get Messages failed with error message\n";
    }
    else
    {
        cout << "Get Messages failed\n";
    }
}

void exitProg(int &csoc)
{
    // Exit send
    char *appmsg = "0000EXIT";
    send(csoc, appmsg, 8, 0);

    // Exit response
    NBResponse *response = new NBResponse(csoc);
    if (strncmp(response->type, "GOOD", 4) == 0)
    {
        cout << "Exit successful\n";
    }
    else if (strncmp(response->type, "ERRM", 4) == 0)
    {
        cout << "Exit failed with error message\n";
    }
    else
    {
        cout << "Exit failed\n";
    }
}

// Client Interface
void clientInterface(int csoc)
{
    cout << "Client Interface\n";
    string username;
    bool loggedIn = false;
    while (true)
    {
        cout << "> ";
        string command;
        vector<string> tokens;
        getline(cin, command);
        int index = 0;

        string token;
        while (index < command.size() && command[index] != ' ')
        {
            token += command[index];
            index++;
        }
        tokens.push_back(token);
        index++;

        if (tokens[0] == "login") // 0040LGIN<username><password>
        {
            login(index, command, csoc, loggedIn, username);
        }
        else if (tokens[0] == "logout") // 0024LOUT<username>
        {
            logout(csoc, loggedIn, username);
        }
        else if (tokens[0] == "post") // <sz>POST<message>
        {
            post(command, csoc, loggedIn);
        }
        else if (tokens[0] == "getm") // 0000GETM
        {
            getMessages(csoc, loggedIn);
        }
        else if (tokens[0] == "exit") // 0008EXIT
        {
            exitProg(csoc);
            break;
        }
        else
        {
            cout << "Invalid command. To exit, call \"exit\".\n";
        }
    }
}