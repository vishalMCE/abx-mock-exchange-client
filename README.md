# ABX Mock Exchange Client

## Prerequisites

Before running the code, make sure you have the required dependencies installed.

Install Boost and nlohmann-json (Ubuntu)
```bash
sudo apt update
sudo apt install libboost-all-dev nlohmann-json3-dev
```

## How to Run the Code Locally
1. Clone the Repo
    ```bash
    git clone https://github.com/your-username/abx-mock-exchange-client.git
    cd abx-mock-exchange-client
    ```
2. Compile the Code
   ```bash
   g++ -std=c++17 abx_client.cpp -o abx_client 
   ```
3. Run the Compiled Executable
   ```bash
   ./abx_client
   ```
