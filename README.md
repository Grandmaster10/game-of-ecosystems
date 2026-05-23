# Sarthaks Game of Market Ecosystems

This is a bare-metal C program designed to run on the ARMv7-DE1-SoC FPGA board for the final Mini Project of the CS2206 Computer Architecture course. It puts an economic twist on the classic Game of Life simulation. Instead of basic cells, the grid simulates regional markets that rely on local trade. These markets can grow their capital, suffer from recessions, pool resources for bailouts, or crash completely from taking on too much risk.

## The Rules of the Simulation

The game runs on a grid where each cell represents a market. Green cells are thriving, while red cells (Vermillion) are failing. The markets change based on the following rules:

* **Survival:** A market with 2 or 3 healthy neighbors will thrive. Its capital grows steadily over time.


* **Recession:** A market with less than 2 healthy neighbors does not have enough trade to survive. It enters a state of decay, turns red, and loses capital.


* **Competition:** A market with more than 3 healthy neighbors dies instantly due to overcrowding.


* **Bailout:** If a dead or decaying market has exactly 3 healthy neighbors, those neighbors will inject capital to revive it.


* **Leverage Risk:** If a market stays healthy for consecutive turns, its leverage (risk) increases. If the leverage hits the critical limit of 6, the market instantly crashes to zero capital.



## Hardware Features Used

Because this program runs without an operating system, it talks directly to the physical hardware on the DE1-SoC board:

* **VGA Pixel Buffer:** Draws the colored grid and cells directly to a monitor.


* **VGA Character Buffer:** Draws the text interface, headers, and footers over the screen.


* **7-Segment Displays:** Shows a live count of how many markets are currently alive and how many are decaying.


* **Hardware Switches and Keys:** Used to move the cursor, pause the game, and change settings.



## Board Controls

You can control the simulation using the physical switches and pushbuttons on the DE1-SoC board.

### Cursor Movement

Make sure the game is paused to move the cursor and edit the board.

* **KEY3:** Move Up


* **KEY2:** Move Down


* **KEY1:** Move Left


* **KEY0:** Move Right



### Game Settings (Switches)

* **SW9:** Flip UP to Play, flip DOWN to Pause.


* **SW8:** Flip UP to reset the screen and clear the board.


* **SW7:** Flip UP for Slow speed, flip DOWN for Fast speed.


* **SW2:** Flip UP to view the Information and Rules screen.



### Drawing Tools (Switches)

Make sure the game is paused to use the brushes.

* **SW0 (Draw Brush):** Flip UP to act as a pen. Moving the cursor will create healthy markets with maximum capital.


* **SW1 (Erase Brush):** Flip UP to act as an eraser. Moving the cursor will instantly kill the markets.
