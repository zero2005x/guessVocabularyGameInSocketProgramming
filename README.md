# 字母猜謎遊戲 - 遊戲說明文件

## 簡介
字母猜謎遊戲是一款簡潔的文字猜字遊戲，適合所有年齡層的玩家。這款遊戲由客戶端(`client.cpp`)和伺服器端(`server.cpp`)組成，基於C++環境開發。玩家需通過客戶端猜測隱藏字母，並從伺服器獲得即時遊戲進度反饋。

## 配置要求
### 伺服器端 (`server.cpp`)
- 初始化Winsock，配置TCP監聽套接字。
- 從`dictionary.txt`中加載單詞庫，隨機挑選目標單詞。
- 處理客戶端的猜字請求並實時更新遊戲狀態。

### 客戶端 (`client.cpp`)
- 初始化Winsock並連接到伺服器。
- 接收伺服器發來的遊戲狀態信息。
- 提供介面讓玩家輸入猜測字母並發送至伺服器。

## 遊戲流程
1. 遊戲開始時，伺服器隨機選擇單詞並向客戶端展示其長度（用底線表示未知字母）。
2. 玩家逐一猜測字母。每猜一字母，伺服器檢查其是否存在於單詞中並更新顯示狀態。
3. 玩家的猜測次數有限。若在限定次數內猜出整個單詞，顯示成功訊息；反之，則顯示失敗訊息。
4. 每回合結束後，玩家可選擇是否繼續遊戲。

## 測試指南
### 啟動伺服器
- 編譯並執行`server.cpp`。確保`dictionary.txt`與執行檔在同一目錄下。

### 啟動客戶端
- 編譯並執行`client.cpp`，這將自動連接到伺服器並開始遊戲。

### 遊戲操作
- 根據客戶端提示輸入猜測字母。
- 觀察伺服器回應以獲得遊戲進度反饋。

### 遊戲結束
- 在遊戲詢問“是否再玩一次（Do you want to play again?）”時，回答`no`結束遊戲，或`yes`重新開始。

## 注意事項
- 確保伺服器端和客戶端在Winsock環境下正確配置。
- 由於遊戲基於網絡通信，請確保網絡連接穩定。
- 預設遊戲在本機（127.0.0.1）的8080端口運行。若需更改設置，請相應調整代碼中的IP地址和端口設定。

## 課程作業細節 - 天主教輔仁大學軟創系電腦網路Socket Programming

### 作業要求
- 需提交一個客戶端程序和一個伺服器端程序。
- 需提供一份說明文件，指導如何遊玩及測試遊戲。

### 提交截止時間
- 截止日期：12月15日（星期五）晚上11:59。
- 遲交者成績將打7折計算，遲交限一週。超過12月22日（星期五）晚上11:59視為缺交，成績記為0分。
- 請勿抄襲，一經發現，抄襲者和被抄襲者均記0分，並無補交機會。

### 編譯環境
- Windows 11

### 編譯指令
- `g++ server.cpp -o server -lws2_32`
- `g++ client.cpp -o client -lws2_32`

### 執行程序
- 先執行`./server.exe`，隨後執行`./client.exe`。


# Letter Guessing Game - Instruction Manual

## Introduction
The Letter Guessing Game is a simple and engaging word guessing game suitable for players of all ages. This game consists of a client (`client.cpp`) and a server (`server.cpp`), developed in a C++ environment. Players guess hidden letters through the client interface and receive real-time feedback on their progress from the server.

## Setup Requirements
### Server Side (`server.cpp`)
- Initialize Winsock and set up a TCP listening socket.
- Load a list of words from `dictionary.txt` and randomly select a target word.
- Process guessing requests from the client and update the game status in real-time.

### Client Side (`client.cpp`)
- Initialize Winsock and connect to the server.
- Receive game status information from the server.
- Provide an interface for players to input their guessed letters and send them to the server.

## Gameplay
1. At the start, the server randomly selects a word and shows its length to the client using underscores for unknown letters.
2. Players guess letters one by one. For each guess, the server checks if it's in the word and updates the display.
3. The number of guesses is limited. Players win by guessing the word within the limit; otherwise, a failure message is displayed.
4. After each round, players can choose to continue or end the game.

## Testing Guide
### Starting the Server
- Compile and run `server.cpp`. Ensure `dictionary.txt` is in the same directory as the executable.

### Starting the Client
- Compile and run `client.cpp`, which will automatically connect to the server and start the game.

### How to Play
- Input guessed letters as prompted by the client.
- Observe server responses to get feedback on game progress.

### Ending the Game
- When asked “Do you want to play again?”, answer `no` to end the game or `yes` to start over.

## Important Notes
- Ensure both the server and client are correctly configured in the Winsock environment.
- As the game relies on network communication, ensure a stable internet connection.
- The game is set to run on local host (127.0.0.1) at port 8080. Change IP address and port settings in the code as needed for different setups.

## Coursework Details - Catholic Fu Jen University Software Creation Department, Computer Networking Socket Programming

### Assignment Requirements
- Submit a client program and a server program.
- Provide an instruction manual on how to play and test the game.

### Submission Deadline
- Deadline: December 15 (Friday) at 11:59 PM.
- Late submissions will be graded at 70% of the total score, with a one-week grace period. Submissions after December 22 (Friday) at 11:59 PM will be considered absent and graded as 0.
- Plagiarism is strictly prohibited. If detected, both the copier and the copied will receive a score of 0 with no opportunity for makeup.

### Compilation Environment
- Windows 11

### Compilation Commands
- `g++ server.cpp -o server -lws2_32`
- `g++ client.cpp -o client -lws2_32`

### Execution
- Run `./server.exe` first, followed by `./client.exe`.
