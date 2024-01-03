# Arena Game

## About The Game
Arena Game is a network-based multiplayer game where four players compete in a virtual arena. Each player spawns in one of the arena's corners and aims to be the last one standing by strategically moving and attacking other players.

## How to Play
- Each player can move or attack one tile per turn.
- Moves are executed immediately on the server side, but the positions are updated only after all 4 players move.
- Players can eliminate opponents by attacking them, with attacks having an immediate effect.
- The last player remaining in the arena wins the game.

## Compiling the Game
The game consists of two parts: the server and the client. Both are written in C++ and use the ncurses library for the user interface. The code was tested on Ubuntu, should work on Mac too. To compile the game, follow these steps:

### Prerequisites
- GCC compiler
- ncurses library

### Compile Instructions
1. Clone the repository to your local machine.
2. Navigate to the directory containing the game files.
3. Compile the server and client separately using g++:
   ```bash
   g++ -o server server.cpp -lpthread
   g++ -o client client.cpp -lncurses
   ```
## Running the Game

5. In-game, players can move using the arrow keys and attack using the keys surrounding the 'G' key on the keyboard (E, T, Y, F, H, C, G, B). 
6. The game progresses in turns. Each player chooses to move or attack. Once all players have made their choice, the game updates the arena.
7. The game continues until only one player remains, and the victory screen will display the winner.
8. After the victory screen, the program will close automatically (both the server and client, cleaning everything up before).

## Game Controls

- Movement: Arrow Keys (Up, Down, Left, Right)
- Attack: 
  - E, T, Y (attacks in the top direction)
  - F, H (attacks to the left and right)
  - C, G, B (attacks in the bottom direction)

## Notes

- The game does not allow players to move into tiles occupied by other players.
- The server handles all game logic and updates, ensuring consistent gameplay for all clients.
- It's important to play strategically, as moving can help avoid incoming attacks.

## Credits

Special thanks to everyone who will contribute to testing and providing feedback for the game.

For any queries or suggestions, feel free to open an issue on this repository.

I'm learning C++, so the code isn't as advanced and readable as it should be, so sorry for that - I'm open to criticism!

Thank you for trying out Arena Game, and have fun! :)
