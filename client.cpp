#include <ncurses.h>
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <sstream>
#include <atomic>
#include <chrono>

bool startsWith(const std::string& fullString, const std::string& starting) {
    if (fullString.length() >= starting.length()) {
        return (0 == fullString.compare(0, starting.length(), starting));
    } else {
        return false;
    }
}

class UserInterface {
public:
    void printInstructions(){
        initscr();            // Initialize the window
        clear();              // Clear the screen
        curs_set(0);          // Hide the cursor
        // ASCII Art Title "Arena Game"
        printw(" _______  ______    _______  __    _  _______    _______  _______  __   __  _______ \n");
        printw("|   _   ||    _ |  |       ||  |  | ||   _   |  |       ||   _   ||  |_|  ||       |\n");
        printw("|  |_|  ||   | ||  |    ___||   |_| ||  |_|  |  |    ___||  |_|  ||       ||    ___|\n");
        printw("|       ||   |_||_ |   |___ |       ||       |  |   | __ |       ||       ||   |___ \n");
        printw("|       ||    __  ||    ___||  _    ||       |  |   ||  ||       ||       ||    ___|\n");
        printw("|   _   ||   |  | ||   |___ | | |   ||   _   |  |   |_| ||   _   || ||_|| ||   |___ \n");
        printw("|__| |__||___|  |_||_______||_|  |__||__| |__|  |_______||__| |__||_|   |_||_______|\n\n");

        // Instructions
        printw("Welcome to the Arena Game!\n");
        printw("Instructions:\n");
        printw("1. Four players spawn in each corner of the arena.\n");
        printw("2. Eliminate others by attacking them, last player standing wins.\n");
        printw("3. Attack or move one tile each turn. Moves are shown after the turn.\n");
        printw("4. Moving strategically can help avoid attacks.\n\n");
        refresh();
        clear();
        endwin();
    }

    void startUpScreen(std::string& port, std::string& username, std::string& character) {
        printInstructions();
        initscr();            // Initialize the window
        cbreak();             // Disable line buffering
        echo();               // Echo keypresses to the window
        curs_set(1);          // Show the cursor
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK); // Green for moved
        init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Yellow for not moved/reset list
        init_pair(3, COLOR_RED, COLOR_BLACK);   // Red for eliminated

        int height = 12;
        int width = 50;
        int start_y = (LINES - height) / 2;
        int start_x = (COLS - width) / 2;

        WINDOW* win = newwin(height, width, start_y, start_x);
        box(win, 0, 0);
        wattron(win, COLOR_PAIR(1));

        mvwprintw(win, 2, 2, "Enter the server port: ");
        char portStr[10];
        wgetstr(win, portStr);
        port = portStr;

        mvwprintw(win, 4, 2, "Enter your username: ");
        char usernameStr[50];
        wgetstr(win, usernameStr);
        username = usernameStr;

        mvwprintw(win, 6, 2, "Enter your character (one character only): ");
        char characterStr[2];
        wgetnstr(win, characterStr, 1);  // Limit to 1 character
        character = characterStr;

        wattroff(win, COLOR_PAIR(1));
        wrefresh(win);
        delwin(win);
        endwin();

        noecho();             // Don't echo keypresses to the window
        curs_set(0);          // Hide the cursor
    }

    void showMessage(const std::string& message, int colorPair) {
        initscr();            // Initialize the window
        cbreak();             // Disable line buffering
        start_color();        // Start color functionality
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_BLACK);

        attron(COLOR_PAIR(colorPair));
        mvprintw(LINES / 2, (COLS - message.size()) / 2, "%s", message.c_str());
        mvprintw(LINES / 2 + 1, (COLS - 34) / 2, "Press any key to continue...");
        attroff(COLOR_PAIR(colorPair));
        refresh();
        getch();  // Wait for user input to continue
        clear();
        endwin(); // End ncurses window
    }

    void displayWaitingScreen(const std::string& playerList) {
        clear();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_BLACK);

        mvprintw(0, 0, "Waiting for all players to connect...");

        int line = 1;
        size_t start = 0, end = playerList.find(";");
        while (end != std::string::npos) {
            std::string playerInfo = playerList.substr(start, end - start);
            int color = playerInfo.find(',') != std::string::npos ? 1 : 2; // Green for connected, red for waiting
            attron(COLOR_PAIR(color));
            mvprintw(line++, 0, "%s", playerInfo.c_str());
            attroff(COLOR_PAIR(color));
            start = end + 1;
            end = playerList.find(";", start);
        }

        refresh();
    }
};

class ClientNetwork {
private:
    int sock;
    struct sockaddr_in serv_addr;

public:
    ClientNetwork() : sock(0) {
        serv_addr.sin_family = AF_INET;
    }

    bool connectToServer(const std::string& address, int port) {
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            return false;
        }
        serv_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr) <= 0) {
            return false;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            return false;
        }

        return true;
    }

    void sendData(const std::string& data) {
        send(sock, data.c_str(), data.size(), 0);
    }

    std::string receiveDataBlocking() {
        char buffer[1024] = {0};
        read(sock, buffer, 1024); // Blocking read
        return std::string(buffer);
    }

    void setNonBlocking(bool nonBlocking) {
        int flags = fcntl(sock, F_GETFL, 0);
        if (nonBlocking) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        } else {
            fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
        }
    }

    std::string tryReceiveData() {
        char buffer[1024] = {0};
        ssize_t bytes_read = read(sock, buffer, 1024);
        if (bytes_read > 0) {
            return std::string(buffer);
        }
        return "";
    }

    ~ClientNetwork() {
        if (sock != -1) {
            close(sock);
        }
    }
};

class GameClient {
private:
    UserInterface ui;
    ClientNetwork clientNetwork;
    std::string portStr, username, character;
    std::string playerList;
    std::mutex playerListMutex;
    std::ofstream debugLog;
    std::pair<int, int> playerPosition;
    std::string currentDirection;
    bool waitingForServerResponse;
    std::atomic<bool> keepUpdatingPlayerList;
    std::atomic<bool> keepUpdatingMoveStatus;
    std::thread updateThread;
    std::thread moveStatusThread;
    std::thread serverCommandThread;
    std::vector<std::tuple<std::string, char, bool, bool>> playerMoveStatus;
    std::map<char, std::tuple<int, int, int>> playerPositions; // Global or within GameClient class
    std::atomic<bool> isGameRunning;

    // Helper function to check if a string is an integer
    bool isInteger(const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
    }

    void displayMoveStatus() {
        int line = 1;
        // Clear previous move list display
        for (int i = line; i < line + playerMoveStatus.size(); ++i) {
            move(i, 0);
            clrtoeol();
        }
        mvprintw(line++, 0, "Player move list:");
        for (const auto& [username, character, hasMoved, isEliminated] : playerMoveStatus) {
            if (isEliminated) {
                attron(COLOR_PAIR(1)); // Red for eliminated
                mvprintw(line++, 0, "%s (%c) [ELIMINATED]", username.c_str(), character);
                attroff(COLOR_PAIR(3));
            } else if (hasMoved) {
                attron(COLOR_PAIR(2)); // Green if moved
                mvprintw(line++, 0, "%s (%c)", username.c_str(), character);
                attroff(COLOR_PAIR(1));
            } else {
                attron(COLOR_PAIR(4)); // Yellow for not moved
                mvprintw(line++, 0, "%s (%c)", username.c_str(), character);
                attroff(COLOR_PAIR(2));
            }
        }
        refresh(); // Force refresh
    }

    void drawKeyMappingsBox() {
        int mappingHeight = 7; // Height of the mapping box
        int mappingWidth = 50; // Width of the mapping box
        int mappingStartY = LINES - mappingHeight - 1; // Position near the bottom
        int mappingStartX = 2; // A little padding from the left edge

        WINDOW* mappingWin = newwin(mappingHeight, mappingWidth, mappingStartY, mappingStartX);
        box(mappingWin, 0, 0); // Draw a box around the window

        // Use cyan color for the key mappings
        wattron(mappingWin, COLOR_PAIR(5));
        mvwprintw(mappingWin, 1, 2, "Attack keys:      |       Movement keys:");
        mvwprintw(mappingWin, 3, 2, " E T Y            |       Up, down, left, right arrow keys");
        mvwprintw(mappingWin, 4, 2, " F   H            |       ");
        mvwprintw(mappingWin, 5, 2, " C G B            |       ");
        wattroff(mappingWin, COLOR_PAIR(5));

        wrefresh(mappingWin); // Refresh the window to show the box and text
        delwin(mappingWin); // Delete the window to avoid memory leaks
    }


    void processMoveStatus(const std::string& moveStatus) {
        std::istringstream moveStream(moveStatus.substr(11)); // Skip "MoveStatus:" prefix
        std::string playerInfo;
        playerMoveStatus.clear();
        while (std::getline(moveStream, playerInfo, ';')) {
            if (playerInfo.empty()) continue;

            std::istringstream infoStream(playerInfo);
            std::string username, charStr, hasMovedStr;
            std::getline(infoStream, username, ',');
            std::getline(infoStream, charStr, ',');
            std::getline(infoStream, hasMovedStr, ',');

            bool hasMoved = hasMovedStr == "1";
            playerMoveStatus.push_back(std::make_tuple(username, charStr[0], hasMoved, false));
        }
        displayMoveStatus();
    }

    void updateMoveStatusThread() {
        while (keepUpdatingMoveStatus) {
            std::string moveStatus = clientNetwork.receiveDataBlocking();
            if (!moveStatus.empty()) {
                std::lock_guard<std::mutex> guard(playerListMutex);
                // Process the move status update
                processMoveStatus(moveStatus);
                // Display the updated move status
                displayMoveStatus();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Refresh every 0.5 seconds
        }
    }

    void updatePlayerListThread() {
        while (keepUpdatingPlayerList) {
            std::string newData = clientNetwork.receiveDataBlocking();
            if (!newData.empty()) {
                std::lock_guard<std::mutex> guard(playerListMutex);
                playerList = newData;
            }
        }
    }

    void displayPlayerDirections(const std::vector<std::pair<std::string, bool>>& playerDirections) {
        int line = 0;
        mvprintw(line++, 0, "Player move list:");
        for (const auto& [username, hasMoved] : playerDirections) {
            attron(COLOR_PAIR(hasMoved ? 2 : 1)); // Green if moved, red otherwise
            mvprintw(line++, 0, "%s, %c", username.c_str(), hasMoved ? 'Y' : 'N');
            attroff(COLOR_PAIR(hasMoved ? 2 : 1));
        }
        refresh();
    }

    void displayVictoryScreen(const std::string& winner) {
        clientNetwork.sendData("VLPDR_DRTBRT"); // Send server shutdown command
        clear();
        start_color();
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // Color for victory message

        attron(COLOR_PAIR(6));
        mvprintw(LINES / 2, (COLS - winner.size() - 17) / 2, "Victory! Winner: %s", winner.c_str());
        mvprintw(LINES / 2 + 1, (COLS - 30) / 2, "Press any key to exit...");
        attroff(COLOR_PAIR(6));

        refresh();
        getch(); // Wait for user input to continue
        clear();
        endwin(); // End ncurses window

        isGameRunning = false;
    }

    void drawArenaAndPlayers() {
        clear();

        if (!has_colors()) {
            printw("Your terminal does not support color");
            getch();
            return;
        }

        start_color();

        // Initialize color pairs for players
        init_pair(1, COLOR_RED, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_BLUE, COLOR_BLACK);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_CYAN, COLOR_BLACK); // For key mappings

        // Display key mappings near the bottom left corner
        int mappingHeight = 8; // Adjusted height of the mapping box
        int mappingWidth = 55; // Width of the mapping box
        int mappingStartY = LINES - mappingHeight - 2; // Adjusted position near the bottom
        int mappingStartX = 1; // A little padding from the left edge

        // Draw the top horizontal line, one character longer to the left
        mvhline(mappingStartY - 1, mappingStartX, ACS_HLINE, mappingWidth + 1); // Horizontal line

        // Draw a vertical line replacing '|' in the key mappings, extending from the top horizontal line
        mvvline(mappingStartY, mappingStartX + 17, ACS_VLINE, mappingHeight); // Vertical line

        // Draw a vertical line on the right, 1 longer on the bottom, and 2 shorter on the top
        mvvline(mappingStartY - 1, mappingStartX + mappingWidth, ACS_VLINE, mappingHeight + 2); // Vertical line down

        // Use cyan color for the key mappings
        attron(COLOR_PAIR(5));
        mvprintw(mappingStartY + 1, mappingStartX, "Attack keys:      ");
        mvprintw(mappingStartY + 1, mappingStartX + 21, "Movement keys:");
        mvprintw(mappingStartY + 3, mappingStartX, " E T Y            ");
        mvprintw(mappingStartY + 3, mappingStartX + 21, "Up, down, left, right arrow keys");
        mvprintw(mappingStartY + 5, mappingStartX, " F   H            ");
        mvprintw(mappingStartY + 6, mappingStartX, " C G B            ");
        attroff(COLOR_PAIR(5));

        // Draw the arena
        const int arenaHeight = 10;
        const int arenaWidth = 24;
        int startY = (LINES - arenaHeight) / 2;
        int startX = (COLS - arenaWidth) / 2;

        // Drawing the arena using lines and corners
        mvaddch(startY, startX, ACS_ULCORNER);  // Upper left corner
        mvaddch(startY, startX + arenaWidth - 1, ACS_URCORNER);  // Upper right corner
        mvaddch(startY + arenaHeight - 1, startX, ACS_LLCORNER);  // Lower left corner
        mvaddch(startY + arenaHeight - 1, startX + arenaWidth - 1, ACS_LRCORNER);  // Lower right corner

        for (int x = startX + 1; x < startX + arenaWidth - 1; x++) {
            mvaddch(startY, x, ACS_HLINE);  // Top border
            mvaddch(startY + arenaHeight - 1, x, ACS_HLINE);  // Bottom border
        }

        for (int y = startY + 1; y < startY + arenaHeight - 1; y++) {
            mvaddch(y, startX, ACS_VLINE);  // Left border
            mvaddch(y, startX + arenaWidth - 1, ACS_VLINE);  // Right border
        }

        debugLog << "Updating player positions in arena." << std::endl;

        for (const auto& [playerChar, posData] : playerPositions) {
            auto [x, y, colorPair] = posData;
            int arenaX = startX + x - 1; // Adjust for the arena's starting position
            int arenaY = startY + y - 1;

            attron(COLOR_PAIR(colorPair));
            mvaddch(arenaY, arenaX, playerChar);
            attroff(COLOR_PAIR(colorPair));
        }

        drawKeyMappingsBox();
        refresh();
    }

    bool allPlayersConnected() {
        size_t start = 0, end = 0;
        int connectedPlayers = 0;

        while ((end = playerList.find(';', start)) != std::string::npos) {
            std::string playerEntry = playerList.substr(start, end - start);
            if (playerEntry.find(',') != std::string::npos && playerEntry.back() != ' ') {
                connectedPlayers++;
            }
            start = end + 1;
        }

        return connectedPlayers >= 4;
    }

    void handleMovement(int ch) {
        if (waitingForServerResponse) return; // Don't handle new input

        std::string command;
        switch (ch) {
            case KEY_UP:    command = "UP"; break;
            case KEY_DOWN:  command = "DOWN"; break;
            case KEY_LEFT:  command = "LEFT"; break;
            case KEY_RIGHT: command = "RIGHT"; break;
            case 'e': case 'E': command = "E"; break; // Attack top left
            case 't': case 'T': command = "T"; break; // Attack top
            case 'y': case 'Y': command = "Y"; break; // Attack top right
            case 'f': case 'F': command = "F"; break; // Attack left
            case 'h': case 'H': command = "H"; break; // Attack right
            case 'c': case 'C': command = "C"; break; // Attack bottom left
            case 'g': case 'G': command = "G"; break; // Attack bottom
            case 'b': case 'B': command = "B"; break; // Attack bottom right
            case '\n':
                if (!currentDirection.empty()) {
                    clientNetwork.sendData(currentDirection);
                    waitingForServerResponse = true;
                    currentDirection = "";
                }
                return;
            default: return;
        }

        if (!command.empty()) {
            currentDirection = command;
            mvprintw(12, 26, "Command entered: %s  ", currentDirection.c_str());
            refresh();
        }
    }

    void updatePlayerPosition(const std::string& updatedPos) {
        // Parse the updated position
        size_t commaPos = updatedPos.find(',');
        int newX = std::stoi(updatedPos.substr(0, commaPos));
        int newY = std::stoi(updatedPos.substr(commaPos + 1));
        playerPosition = {newX, newY};
    }

    void updatePlayerPositions(const std::string& positionsData) {
        debugLog << "Updating positions with data: " << positionsData << std::endl;

        std::istringstream playerStream(positionsData);
        std::string playerInfo;

        playerPositions.clear(); // Clear previous positions

        while (std::getline(playerStream, playerInfo, ';')) {
            if (playerInfo.empty()) continue;

            debugLog << "Processing player info: " << playerInfo << std::endl;

            std::istringstream infoStream(playerInfo);
            std::string xStr, yStr, charStr, colorPairStr;

            std::getline(infoStream, xStr, ',');
            std::getline(infoStream, yStr, ',');
            std::getline(infoStream, charStr, ',');

            if (!std::getline(infoStream, colorPairStr, ',') || colorPairStr.empty()) {
                debugLog << "Invalid or missing color pair: " << playerInfo << std::endl;
                continue; // Skip this player info if the color pair is missing or invalid
            }

            try {
                int x = std::stoi(xStr);
                int y = std::stoi(yStr);
                char playerChar = !charStr.empty() ? charStr.front() : ' ';
                int colorPair = std::stoi(colorPairStr);

                playerPositions[playerChar] = std::make_tuple(x, y, colorPair);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid data received for player position: " << playerInfo << std::endl;
            } catch (const std::out_of_range& e) {
                std::cerr << "Out of range data received for player position: " << playerInfo << std::endl;
            }
        }

        drawArenaAndPlayers(); // Call to update the arena
        displayMoveStatus();
    }

    void resetMoveList() {
        for (auto& [username, charInList, hasMoved, isEliminated] : playerMoveStatus) {
            if (!isEliminated) { // Only reset if not eliminated
                hasMoved = false;
            }
        }
        displayMoveStatus();
    }

    void updateMoveStatus(char playerChar) {
        for (auto& [username, charInList, hasMoved, isEliminated] : playerMoveStatus) {
            if (charInList == playerChar && !isEliminated) {
                hasMoved = true;
                break;
            }
        }
        displayMoveStatus();
    }

    void handleElimination(char eliminatedPlayerChar) {
        // Remove the eliminated player from the arena
        playerPositions.erase(eliminatedPlayerChar);

        // Update the elimination status in the player move list
        for (auto& [username, charInList, hasMoved, isEliminated] : playerMoveStatus) {
            if (charInList == eliminatedPlayerChar) {
                isEliminated = true;
                break;
            }
        }

        // Update the arena display and move list
        drawArenaAndPlayers();
        displayMoveStatus();
    }

    void handleServerCommands() {
        while (true) {
            if(playerPositions.size() == 1){
                // Get the username of the remaining player
                char lastPlayerChar = playerPositions.begin()->first;
                for (const auto& [username, charInList, hasMoved, isEliminated] : playerMoveStatus) {
                    if (charInList == lastPlayerChar) {
                        displayVictoryScreen(username);
                        break;
                    }
                }
                // Close the client
                clientNetwork.~ClientNetwork();
                std::cout << "Client shutdown initiated." << std::endl;
                isGameRunning = false;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                return;
            }

            std::string command = clientNetwork.tryReceiveData();
            if (!command.empty()) {
                debugLog << "Received command: " << command << std::endl;

                std::istringstream commandsStream(command);
                std::string singleCommand;
                while (std::getline(commandsStream, singleCommand, '|')) {
                    if (singleCommand.empty()) continue;

                    char commandType = singleCommand[0];
                    std::string commandData = singleCommand.substr(1);

                    switch (commandType) {
                        case 'L':
                            updateMoveStatus(commandData[0]); // Update the move status for this player
                            break;
                        case 'R':
                            resetMoveList(); // Reset the move list to red
                            break;
                        case 'P':
                            updatePlayerPositions(commandData);
                            waitingForServerResponse = false;
                            break;
                        case 'E':
                            handleElimination(commandData[0]);
                            break;
                        default:
                            break;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void initializePlayerDirectionList() {
        std::istringstream initStream(playerList);
        std::string playerInfo;
        playerMoveStatus.clear();
        while (std::getline(initStream, playerInfo, ';')) {
            if (playerInfo.empty()) continue;
            size_t commaPos = playerInfo.find(',');
            if (commaPos != std::string::npos) {
                std::string username = playerInfo.substr(0, commaPos);
                char character = playerInfo[commaPos + 2];
                playerMoveStatus.push_back(std::make_tuple(username, character, false, false));
            }
        }
    }

public:
    GameClient() : keepUpdatingPlayerList(true), isGameRunning(true){
        debugLog.open("debug.log.txt");
        waitingForServerResponse = false;
        keepUpdatingMoveStatus = true;
    }

    ~GameClient() {
        keepUpdatingPlayerList = false; // Ensure the thread stops
        if (updateThread.joinable()) {
            updateThread.join(); // Wait for the thread to finish
        }
        if(debugLog.is_open()){
            debugLog.close();
        }
    }

    void drawInitialPlayerPositions() {
        clear();
        start_color();

        // Initialize color pairs for players
        init_pair(1, COLOR_RED, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_BLUE, COLOR_BLACK);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_CYAN, COLOR_BLACK); // For key mappings

        // Display key mappings near the bottom left corner
        int mappingHeight = 8; // Adjusted height of the mapping box
        int mappingWidth = 55; // Width of the mapping box
        int mappingStartY = LINES - mappingHeight - 2; // Adjusted position near the bottom
        int mappingStartX = 1; // A little padding from the left edge

        // Draw the top horizontal line, one character longer to the left
        mvhline(mappingStartY - 1, mappingStartX, ACS_HLINE, mappingWidth + 1); // Horizontal line

        // Draw a vertical line replacing '|' in the key mappings, extending from the top horizontal line
        mvvline(mappingStartY, mappingStartX + 17, ACS_VLINE, mappingHeight); // Vertical line

        // Draw a vertical line on the right, 1 longer on the bottom, and 2 shorter on the top
        mvvline(mappingStartY - 1, mappingStartX + mappingWidth, ACS_VLINE, mappingHeight + 2); // Vertical line down

        // Use cyan color for the key mappings
        attron(COLOR_PAIR(5));
        mvprintw(mappingStartY + 1, mappingStartX, "Attack keys:      ");
        mvprintw(mappingStartY + 1, mappingStartX + 21, "Movement keys:");
        mvprintw(mappingStartY + 3, mappingStartX, " E T Y            ");
        mvprintw(mappingStartY + 3, mappingStartX + 21, "Up, down, left, right arrow keys");
        mvprintw(mappingStartY + 5, mappingStartX, " F   H            ");
        mvprintw(mappingStartY + 6, mappingStartX, " C G B            ");
        attroff(COLOR_PAIR(5));

        // Draw the arena
        const int arenaHeight = 10;
        const int arenaWidth = 24;
        int startY = (LINES - arenaHeight) / 2;
        int startX = (COLS - arenaWidth) / 2;

        // Drawing the arena using lines and corners
        mvaddch(startY, startX, ACS_ULCORNER);  // Upper left corner
        mvaddch(startY, startX + arenaWidth - 1, ACS_URCORNER);  // Upper right corner
        mvaddch(startY + arenaHeight - 1, startX, ACS_LLCORNER);  // Lower left corner
        mvaddch(startY + arenaHeight - 1, startX + arenaWidth - 1, ACS_LRCORNER);  // Lower right corner

        for (int x = startX + 1; x < startX + arenaWidth - 1; x++) {
            mvaddch(startY, x, ACS_HLINE);  // Top border
            mvaddch(startY + arenaHeight - 1, x, ACS_HLINE);  // Bottom border
        }

        for (int y = startY + 1; y < startY + arenaHeight - 1; y++) {
            mvaddch(y, startX, ACS_VLINE);  // Left border
            mvaddch(y, startX + arenaWidth - 1, ACS_VLINE);  // Right border
        }

        // Extract player characters from playerList
        std::istringstream playerStream(playerList);
        std::string playerInfo;
        std::vector<char> playerChars;
        while (std::getline(playerStream, playerInfo, ';')) {
            size_t commaPos = playerInfo.find(',');
            if (commaPos != std::string::npos && commaPos + 2 < playerInfo.size()) {
                char playerChar = playerInfo[commaPos + 2]; // Extract the character after the comma and space
                playerChars.push_back(playerChar);
                debugLog << "Extracted player character: " << playerChar << std::endl; // Log each extracted character
            }
        }

        // Ensure we have exactly 4 characters before drawing them
        if (playerChars.size() != 4) {
            debugLog << "Error: Expected 4 player characters, but got " << playerChars.size() << std::endl;
            return;
        }

        // Predefined corner positions (adjusted to be within the arena)
        std::vector<std::pair<int, int>> cornerPositions = {
                {startY + 1, startX + 1},           // Top-left
                {startY + 1, startX + arenaWidth - 2}, // Top-right
                {startY + arenaHeight - 2, startX + 1}, // Bottom-left
                {startY + arenaHeight - 2, startX + arenaWidth - 2} // Bottom-right
        };

        // Draw each player in a corner
        for (size_t i = 0; i < playerChars.size(); ++i) {
            int arenaY = cornerPositions[i].first;   // y-coordinate
            int arenaX = cornerPositions[i].second;  // x-coordinate

            attron(COLOR_PAIR(i + 1));
            mvaddch(arenaY, arenaX, playerChars[i]); // Draw the player character

            debugLog << "Drawing player: " << playerChars[i] << std::endl;
            attroff(COLOR_PAIR(i + 1));
        }

        refresh();
    }

    void run() {
        // Start-up UI to get server port, username, and character
        ui.startUpScreen(portStr, username, character);

        // Connect to the server
        if (!clientNetwork.connectToServer("127.0.0.1", std::stoi(portStr))) {
            std::cout << "Failed to connect to server." << std::endl;
            return;
        }

        // Send player info to server
        std::string data = username + "," + character;
        clientNetwork.sendData(data);

        // Receive initial player list from server
        playerList = clientNetwork.receiveDataBlocking();
        if (playerList == "taken") {
            std::cout << "Username or character already taken." << std::endl;
            return;
        }

        // Initialize player move status based on the received player list
        std::istringstream initStream(playerList);
        std::string playerInfo;
        while (std::getline(initStream, playerInfo, ';')) {
            size_t commaPos = playerInfo.find(',');
            if (commaPos != std::string::npos) {
                std::string username = playerInfo.substr(0, commaPos);
                char character = playerInfo[commaPos + 2];
                playerMoveStatus.push_back(std::make_tuple(username, character, false, false));
            }
        }

        // Set the client network to non-blocking mode
        clientNetwork.setNonBlocking(true);

        // Start a thread to update the player list
        std::thread updateThread(&GameClient::updatePlayerListThread, this);

        // Initialize ncurses for the main loop
        initscr();

        // Wait for all players to connect
        while (!allPlayersConnected()) {
            std::string currentList;
            {
                std::lock_guard<std::mutex> guard(playerListMutex);
                currentList = playerList;
            }
            ui.displayWaitingScreen(currentList);
            napms(500); // Refresh every 500 ms
        }

        // All players connected, proceed to the game
        keepUpdatingPlayerList = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }

        initializePlayerDirectionList();

        // Start the thread to handle server commands
        serverCommandThread = std::thread(&GameClient::handleServerCommands, this);

        // Draw initial player positions and arena
        drawInitialPlayerPositions();
//        drawArenaAndPlayers(playerList);
        displayMoveStatus();

        // Movement handling loop
        keypad(stdscr, TRUE); // Enable arrow keys
        while (isGameRunning) {
            int ch = getch(); // Get user input (blocking)
            if (ch == 'q' || ch == 'Q') break; // Quit on 'q'
            handleMovement(ch);
        }

        // End ncurses mode
        endwin();
        std::cout << "Exiting the game." << std::endl;

        // Ensure the server command thread is properly closed
        if (serverCommandThread.joinable()) {
            serverCommandThread.join();
        }
    }
};

int main() {
    GameClient gameClient;
    gameClient.run();
    return 0;
}