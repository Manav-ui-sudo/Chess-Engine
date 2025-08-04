#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

using namespace std;

enum Piece
{
    EMPTY = 0,
    WPAWN = 1,
    WKNIGHT = 2,
    WBISHOP = 3,
    WROOK = 4,
    WQUEEN = 5,
    WKING = 6,
    BPAWN = -1,
    BKNIGHT = -2,
    BBISHOP = -3,
    BROOK = -4,
    BQUEEN = -5,
    BKING = -6
};

class ChessBoard
{
public:
    int board[64];
    bool whiteToMove;

    ChessBoard() { reset(); }

    void reset()
    {
        int initial[] = {
            BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BKNIGHT, BROOK,
            BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN,
            EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
            EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
            EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
            EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
            WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN,
            WROOK, WKNIGHT, WBISHOP, WQUEEN, WKING, WBISHOP, WKNIGHT, WROOK};
        copy(begin(initial), end(initial), board);
        whiteToMove = true;
    }

    string squareToAlgebraic(int sq)
    {
        return string(1, 'a' + (sq % 8)) + string(1, '8' - (sq / 8));
    }

    int squareFromAlgebraic(const string &s)
    {
        return (8 - (s[1] - '0')) * 8 + (s[0] - 'a');
    }

    bool isWhite(int piece) const { return piece > 0; }
    bool isBlack(int piece) const { return piece < 0; }

    bool makeMove(const string &uciMove)
    {
        int from = squareFromAlgebraic(uciMove.substr(0, 2));
        int to = squareFromAlgebraic(uciMove.substr(2, 2));
        int piece = board[from];

        if ((whiteToMove && !isWhite(piece)) || (!whiteToMove && !isBlack(piece)))
            return false;

        auto legalMoves = generateMoves();
        for (auto &move : legalMoves)
        {
            if (move.first == from && move.second == to)
            {
                board[to] = piece;
                board[from] = EMPTY;
                whiteToMove = !whiteToMove;
                return true;
            }
        }
        return false;
    }

    vector<pair<int, int>> generateMoves()
    {
        vector<pair<int, int>> moves;
        int knightOffsets[] = {17, 15, 10, 6, -6, -10, -15, -17};
        int directions[] = {1, -1, 8, -8, 7, -7, 9, -9};

        for (int i = 0; i < 64; ++i)
        {
            int piece = board[i];
            if (piece == EMPTY)
                continue;
            if ((whiteToMove && isBlack(piece)) || (!whiteToMove && isWhite(piece)))
                continue;

            if (abs(piece) == WPAWN)
            {
                int dir = whiteToMove ? -8 : 8;
                int oneAhead = i + dir;
                if (inBounds(i, oneAhead) && board[oneAhead] == EMPTY)
                    moves.emplace_back(i, oneAhead);
                // Double move
                int startRow = whiteToMove ? 6 : 1;
                if (i / 8 == startRow && board[oneAhead] == EMPTY && board[oneAhead + dir] == EMPTY)
                    moves.emplace_back(i, oneAhead + dir);
                // Captures
                for (int dx : {-1, 1})
                {
                    int target = i + dir + dx;
                    if (inBounds(i, target) && board[target] != EMPTY &&
                        ((whiteToMove && isBlack(board[target])) || (!whiteToMove && isWhite(board[target]))))
                        moves.emplace_back(i, target);
                }
            }
            else if (abs(piece) == WKNIGHT)
            {
                for (int d : knightOffsets)
                {
                    int to = i + d;
                    if (inBounds(i, to) && (board[to] == EMPTY ||
                                            (whiteToMove && isBlack(board[to])) || (!whiteToMove && isWhite(board[to]))))
                        moves.emplace_back(i, to);
                }
            }
            else if (abs(piece) == WBISHOP || abs(piece) == WROOK || abs(piece) == WQUEEN)
            {
                int start = (abs(piece) == WBISHOP) ? 4 : (abs(piece) == WROOK ? 0 : 0);
                int end = (abs(piece) == WBISHOP) ? 8 : (abs(piece) == WROOK ? 4 : 8);
                for (int d = start; d < end; ++d)
                {
                    int to = i;
                    while (true)
                    {
                        int next = to + directions[d];
                        if (!inBounds(to, next))
                            break;
                        if (board[next] != EMPTY)
                        {
                            if ((whiteToMove && isBlack(board[next])) || (!whiteToMove && isWhite(board[next])))
                                moves.emplace_back(i, next);
                            break;
                        }
                        moves.emplace_back(i, next);
                        to = next;
                    }
                }
            }
            else if (abs(piece) == WKING)
            {
                for (int d = 0; d < 8; ++d)
                {
                    int to = i + directions[d];
                    if (inBounds(i, to) && (board[to] == EMPTY ||
                                            (whiteToMove && isBlack(board[to])) || (!whiteToMove && isWhite(board[to]))))
                        moves.emplace_back(i, to);
                }
            }
        }
        return moves;
    }

    bool inBounds(int from, int to)
    {
        return to >= 0 && to < 64 && abs((from % 8) - (to % 8)) <= 2;
    }

    int evaluate() const
    {
        int score = 0;
        for (int i = 0; i < 64; ++i)
        {
            int piece = board[i];
            score += piece;
        }
        return whiteToMove ? score : -score;
    }
};

class Engine
{
public:
    ChessBoard &board;

    Engine(ChessBoard &b) : board(b) {}

    pair<int, pair<int, int>> alphaBeta(int depth, int alpha, int beta, bool maximizing)
    {
        if (depth == 0)
            return {board.evaluate(), {-1, -1}};

        auto moves = board.generateMoves();
        if (moves.empty())
            return {board.evaluate(), {-1, -1}};

        pair<int, int> bestMove = moves[0];
        int bestEval = maximizing ? -1e9 : 1e9;

        for (auto &move : moves)
        {
            ChessBoard temp = board;
            string uci = board.squareToAlgebraic(move.first) + board.squareToAlgebraic(move.second);
            temp.makeMove(uci);

            int eval = alphaBeta(depth - 1, alpha, beta, !maximizing).first;
            if (maximizing && eval > bestEval)
            {
                bestEval = eval;
                bestMove = move;
                alpha = max(alpha, eval);
            }
            else if (!maximizing && eval < bestEval)
            {
                bestEval = eval;
                bestMove = move;
                beta = min(beta, eval);
            }
            if (beta <= alpha)
                break;
        }
        return {bestEval, bestMove};
    }

    string getBestMove()
    {
        auto result = alphaBeta(3, -1e9, 1e9, board.whiteToMove);
        return board.squareToAlgebraic(result.second.first) + board.squareToAlgebraic(result.second.second);
    }
};

void uciLoop()
{
    ChessBoard board;
    Engine engine(board);
    string line;
    while (getline(cin, line))
    {
        istringstream iss(line);
        string cmd;
        iss >> cmd;
        if (cmd == "uci")
        {
            cout << "id name SimpleChess\nid author ChatGPT\nuciok\n";
        }
        else if (cmd == "isready")
        {
            cout << "readyok\n";
        }
        else if (cmd == "position")
        {
            string type;
            iss >> type;
            if (type == "startpos")
            {
                board.reset();
                string token;
                if (iss >> token && token == "moves")
                {
                    string move;
                    while (iss >> move)
                        board.makeMove(move);
                }
            }
        }
        else if (cmd == "go")
        {
            string bestMove = engine.getBestMove();
            cout << "bestmove " << bestMove << "\n";
        }
        else if (cmd == "quit")
        {
            break;
        }
    }
}

int main()
{
    uciLoop();
    return 0;
}
