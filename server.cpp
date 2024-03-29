#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctime>
#include <algorithm>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <chrono>

class Player {
public:
    std::string username;
    char character;
    int x, y;
    int colorPair;
    bool hasMoved = false;
    std::string lastDirection;
    bool eliminated = false;

    Player(const std::string& username, char character, int x, int y, int colorPair) :
            username(username), character(character), x(x), y(y), colorPair(colorPair) {}
};

class ServerNetwork {
private:
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

public:
    ServerNetwork() {
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = 0;  // Let OS choose the port

        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
        bind(server_fd, (struct sockaddr *)&address, sizeof(address));

        socklen_t len = sizeof(address);
        getsockname(server_fd, (struct sockaddr *)&address, &len);
        listen(server_fd, 3);

        std::cout << "Server is running on port " << ntohs(address.sin_port) << std::endl;
    }

    int acceptClient(struct sockaddr_in& client_addr) {
        socklen_t addrlen = sizeof(client_addr);
        return accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
    }

    void setNonBlocking(int socket){
        int flags = fcntl(socket, F_GETFL, 0);
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    }

    ~ServerNetwork() {
        close(server_fd);
    }
};

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X"); // Format: YYYY-MM-DD HH:mm:ss
    return ss.str();
}

bool isUsernameOrCharacterTaken(const std::string& username, char character, const std::vector<Player*>& players) {
    for (const auto& player : players) {
        if (player->username == username || player->character == character) {
            return true;
        }
    }
    return false;
}

std::string createPlayerList(const std::vector<Player*>& players) {
    std::string playerList;
    for (int i = 0; i < 4; ++i) {
        if (i < players.size()) {
            playerList += players[i]->username + ", " + players[i]->character + ";";
        } else {
            playerList += "Player " + std::to_string(i + 1) + ";";
        }
    }
    return playerList;
}

void logConnection(const std::string& username, char character) {
    std::cout << "[" << getCurrentTimestamp() << "] New connection: Username = " << username << ", Character = " << character << std::endl;
}

bool isPositionOccupied(int x, int y, const std::vector<Player*>& players) {
    for (const auto& player : players) {
        if (!player->eliminated && player->x == x && player->y == y) {
            return true;
        }
    }
    return false;
}

void updatePlayerPosition(Player* player, const std::string& command, const std::vector<Player*>& players) {
    if (command.empty()) return;

    char direction = command[0];

    int newX = player->x, newY = player->y;

    if (direction == 'U') newY -= 1;
    else if (direction == 'D') newY += 1;
    else if (direction == 'L') newX -= 1;
    else if (direction == 'R') newX += 1;

    // Boundary checks
    if (newX > 1 && newX < 24 && newY > 1 && newY < 10 && !isPositionOccupied(newX, newY, players)) {
        player->x = newX;
        player->y = newY;
    }

    // Log updated position
//    std::time_t now = std::time(0);
//    std::tm* ltm = std::localtime(&now);
//    std::cout << "[" << 1 + ltm->tm_hour << ":" << 1 + ltm->tm_min << ":" << 1 + ltm->tm_sec
//              << "] Player position updated: " << player->username
//              << " (" << player->x << ", " << player->y << ")" << std::endl;
}

std::string receiveData(int socket) {
    char buffer[1024] = {0};
    ssize_t bytes_read = read(socket, buffer, 1024);
    if (bytes_read > 0) {
        return std::string(buffer, bytes_read);
    }
    return ""; // Return an empty string if no data is received
}

std::string createMoveStatus(const std::vector<Player*>& players) {
    std::string status;
    for (const auto& player : players) {
        status += player->username + "," + player->character + ",";
        status += player->hasMoved ? "1" : "0";
        status += ";";
    }
    return status;
}

bool isAttackCommand(const std::string& command) {
    // Check if the command is one of the attack commands
    std::string attackCommands = "ETFYFHCGBCetfyfhcgbc";
    return attackCommands.find(command) != std::string::npos;
}

void processAttackCommand(Player* attacker, const std::vector<Player*>& players, const std::string& command, const std::unordered_map<int, Player*>& socketToPlayerMap) {
    std::cout << "[" << getCurrentTimestamp() << "] Attack command received from " << attacker->username << ": " << command << std::endl;
    int attackX = attacker->x, attackY = attacker->y;

    if (command == "e" || command == "E") { attackX--; attackY--; } // Attack top left
    else if (command == "t" || command == "T") { attackY--; } // Attack top
    else if (command == "y" || command == "Y") { attackX++; attackY--; } // Attack top right
    else if (command == "f" || command == "F") { attackX--; } // Attack left
    else if (command == "h" || command == "H") { attackX++; } // Attack right
    else if (command == "c" || command == "C") { attackX--; attackY++; } // Attack bottom left
    else if (command == "g" || command == "G") { attackY++; } // Attack bottom
    else if (command == "b" || command == "B") { attackX++; attackY++; } // Attack bottom right

    for (auto& target : players) {
        if (target != attacker && !target->eliminated && target->x == attackX && target->y == attackY) {
            target->eliminated = true; // Eliminate the target
            std::cout << "[" << getCurrentTimestamp() << "] Player " << target->username << " eliminated by " << attacker->username << std::endl;
            // Send update to all clients about the elimination
            std::string eliminationUpdate = "E" + std::string(1, target->character) + "|";
            for (const auto& pair : socketToPlayerMap) {
                send(pair.first, eliminationUpdate.c_str(), eliminationUpdate.length(), 0);
            }
            break; // Only eliminate one player per attack
        }
    }
}

std::string generateMoveStatus(const std::vector<Player*>& players) {
    std::string moveStatus;
    for (const auto& player : players) {
        moveStatus += player->username + "," + player->character + "," + (player->hasMoved ? "1" : "0") + ";";
    }
    return moveStatus;
}

std::string generatePositions(const std::vector<Player*>& players) {
    std::string positions;
    for (const auto& player : players) {
        if (player->eliminated) {
            positions += player->character + std::string("X;"); // Indicate elimination
        } else {
            positions += std::to_string(player->x) + "," + std::to_string(player->y)
                         + "," + player->character + "," + std::to_string(player->colorPair) + ";";
        }
    }
    return positions;
}

void logDirectionReceived(const std::string& username, const std::string& direction) {
    std::cout << "[" << getCurrentTimestamp() << "] Direction received: Username = " << username << ", Direction = " << direction << std::endl;
}

void logPositionUpdate(const std::string& positions) {
    std::cout << "[" << getCurrentTimestamp() << "] Position update: " << positions << std::endl;
}

int main() {
    ServerNetwork serverNetwork;
    std::vector<Player*> players;
    std::unordered_map<int, Player*> socketToPlayerMap; // Maps socket FD to player
    std::unordered_map<int, bool> receivedDirections;   // Tracks whether a direction has been received

    // Positions for players (top-left, top-right, bottom-left, bottom-right)
    std::vector<std::pair<int, int>> startingPositions = {{2, 2}, {23, 2}, {2, 9}, {23, 9}};
    int movedPlayersCount = 0;

    while (players.size() < 4) {
        struct sockaddr_in client_addr;
        int new_socket = serverNetwork.acceptClient(client_addr);

        if (new_socket < 0) {
            continue;
        }

        char buffer[1024] = {0};
        ssize_t bytes_read = read(new_socket, buffer, 1024);
        if (bytes_read <= 0) {
            close(new_socket);
            continue;
        }

        std::string data(buffer);
        size_t commaPos = data.find(',');
        std::string username = data.substr(0, commaPos);
        char character = data[commaPos + 1];

        if (isUsernameOrCharacterTaken(username, character, players)) {
            std::string response = "taken";
            send(new_socket, response.c_str(), response.length(), 0);
            close(new_socket);
            continue;
        }

        logConnection(username, character);  // Log new connection

        Player* newPlayer = new Player(username, character,
                                       startingPositions[players.size()].first,
                                       startingPositions[players.size()].second,
                                       players.size() + 1);
        players.push_back(newPlayer);
        socketToPlayerMap[new_socket] = newPlayer;
        receivedDirections[new_socket] = false;

        std::string playerList = createPlayerList(players);

        for (const auto& pair : socketToPlayerMap) {
            send(pair.first, playerList.c_str(), playerList.length(), 0);
        }

        serverNetwork.setNonBlocking(new_socket);
    }

    // Inside the server main loop
    while (true) {
        bool allDirectionsReceived = true;

        for (auto& pair : socketToPlayerMap) {
            int socket = pair.first;
            Player* player = pair.second;

            if (!receivedDirections[socket] && !player->eliminated) {
                std::string command = receiveData(socket);
                if (command == "VLPDR_DRTBRT"){
                    std::cout << "[" << getCurrentTimestamp() << "] Server shutdown initiated." << std::endl;
                    // Close all client sockets
                    for (const auto& pair : socketToPlayerMap) {
                        close(pair.first);
                    }

                    // Release all dynamically allocated Player objects
                    for (Player* player : players) {
                        delete player;
                    }

                    // Log completion of cleanup
                    std::cout << "[" << getCurrentTimestamp() << "] Server resources cleaned up. Shutting down." << std::endl;

                    return 0;
                }
                if (!command.empty()) {
                    if (isAttackCommand(command)) {
                        processAttackCommand(player, players, command, socketToPlayerMap);
                        receivedDirections[socket] = true;
                    } else {
                        // Update player direction and log it
                        player->lastDirection = command;
                        logDirectionReceived(player->username, command);
                        receivedDirections[socket] = true;
                        player->hasMoved = true;
                        updatePlayerPosition(player, command, players);
                    }

                    // Update list status
                    std::string listUpdate = "L" + std::string(1, player->character) + "|";
                    for (const auto &innerPair: socketToPlayerMap) {
                        send(innerPair.first, listUpdate.c_str(), listUpdate.length(), 0);
                    }
                } else {
                    allDirectionsReceived = false;
                }
            }
        }

        if (allDirectionsReceived) {
            // Prepare 'R|P' command with positions
            std::string positions = generatePositions(players);
            std::string resetAndPositionsCommand = "R|P" + positions + "|";

            // Log the position update
            logPositionUpdate(positions);
            std::cout << "[" << getCurrentTimestamp() << "] Sending position update to clients: " << resetAndPositionsCommand << std::endl;

            // Send reset and positions command to all clients
            for (const auto& pair : socketToPlayerMap) {
                send(pair.first, resetAndPositionsCommand.c_str(), resetAndPositionsCommand.length(), 0);
            }

            // Reset the directions and move status
            for(auto& player : players) {
                player->hasMoved = false;
            }
            for(auto& pair : receivedDirections) {
                pair.second = false;
            }
        }
        usleep(100000); // Small delay
    }

    // Cleanup and close sockets
    for (Player* player : players) {
        delete player;
    }
    for (const auto& pair : socketToPlayerMap) {
        close(pair.first);
    }

    return 0;
}