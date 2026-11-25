// Chess engine: 64-square mailbox board, legal move generation, negamax + alpha-beta, UCI.
// Square index: 0 = a8 ... 7 = h8 ... 56 = a1 ... 63 = h1   (row = i/8, col = i%8)
// Pieces: white positive, black negative (PAWN..KING = 1..6)
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdlib>
using namespace std;

enum Piece { EMPTY = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6 };
enum Castle { WK = 1, WQ = 2, BK = 4, BQ = 8 };

const int VALUE[7] = {0, 100, 320, 330, 500, 900, 0};
const int MATE = 100000;
const int INF = 1000000;
const char *START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

const int DR[8] = {-1, 1, 0, 0, -1, -1, 1, 1}; // 0-3 rook dirs, 4-7 bishop dirs
const int DC[8] = {0, 0, -1, 1, -1, 1, -1, 1};
const int KR[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
const int KC[8] = {-1, 1, -2, 2, -2, 2, -1, 1};

inline bool onBoard(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

struct Move
{
    int from, to, promo; // promo = 0 or KNIGHT..QUEEN
};

// ===================== Board: representation, move generation, make =====================
struct Board
{
    int sq[64];
    bool whiteToMove;
    int castle; // bitmask of Castle
    int ep;     // en-passant target square or -1

    Board() { setFen(START_FEN); }

    void setFen(const string &fen)
    {
        fill(sq, sq + 64, EMPTY);
        istringstream in(fen);
        string place, side, rights, epStr;
        in >> place >> side >> rights >> epStr;
        int i = 0;
        for (char ch : place)
        {
            if (ch == '/') continue;
            if (isdigit(ch)) { i += ch - '0'; continue; }
            int t = 0;
            switch (tolower(ch))
            {
            case 'p': t = PAWN; break;
            case 'n': t = KNIGHT; break;
            case 'b': t = BISHOP; break;
            case 'r': t = ROOK; break;
            case 'q': t = QUEEN; break;
            case 'k': t = KING; break;
            }
            sq[i++] = isupper(ch) ? t : -t;
        }
        whiteToMove = (side != "b");
        castle = 0;
        for (char ch : rights)
        {
            if (ch == 'K') castle |= WK;
            if (ch == 'Q') castle |= WQ;
            if (ch == 'k') castle |= BK;
            if (ch == 'q') castle |= BQ;
        }
        ep = (epStr.size() == 2 && epStr != "-") ? fromAlg(epStr) : -1;
    }

    static int fromAlg(const string &s) { return (8 - (s[1] - '0')) * 8 + (s[0] - 'a'); }
    static string toAlg(int s) { return string(1, 'a' + s % 8) + string(1, '8' - s / 8); }

    // Is square t attacked by the given side?
    bool attacked(int t, bool byWhite) const
    {
        int tr = t / 8, tc = t % 8, s = byWhite ? 1 : -1;
        int pr = tr + (byWhite ? 1 : -1); // an attacking pawn sits one row "behind" the target
        for (int dc : {-1, 1})
            if (onBoard(pr, tc + dc) && sq[pr * 8 + tc + dc] == s * PAWN) return true;
        for (int k = 0; k < 8; k++)
        {
            int r = tr + KR[k], c = tc + KC[k];
            if (onBoard(r, c) && sq[r * 8 + c] == s * KNIGHT) return true;
        }
        for (int d = 0; d < 8; d++)
        {
            int r = tr + DR[d], c = tc + DC[d];
            if (onBoard(r, c) && sq[r * 8 + c] == s * KING) return true;
            while (onBoard(r, c))
            {
                int p = sq[r * 8 + c];
                if (p != EMPTY)
                {
                    if (p == s * QUEEN || (d < 4 && p == s * ROOK) || (d >= 4 && p == s * BISHOP)) return true;
                    break;
                }
                r += DR[d]; c += DC[d];
            }
        }
        return false;
    }

    int kingSquare(bool white) const
    {
        for (int i = 0; i < 64; i++)
            if (sq[i] == (white ? KING : -KING)) return i;
        return -1;
    }

    bool inCheck() const { return attacked(kingSquare(whiteToMove), !whiteToMove); }

    void pseudoMoves(vector<Move> &out) const
    {
        int me = whiteToMove ? 1 : -1;
        for (int i = 0; i < 64; i++)
        {
            int p = sq[i];
            if (p * me <= 0) continue;
            int type = abs(p), r = i / 8, c = i % 8;
            if (type == PAWN)
            {
                int dir = whiteToMove ? -1 : 1, startRow = whiteToMove ? 6 : 1, promoRow = whiteToMove ? 0 : 7;
                int nr = r + dir;
                if (!onBoard(nr, c)) continue;
                auto add = [&](int to) {
                    if (to / 8 == promoRow) for (int pr : {QUEEN, ROOK, BISHOP, KNIGHT}) out.push_back({i, to, pr});
                    else out.push_back({i, to, 0});
                };
                if (sq[nr * 8 + c] == EMPTY)
                {
                    add(nr * 8 + c);
                    if (r == startRow && sq[(nr + dir) * 8 + c] == EMPTY) out.push_back({i, (nr + dir) * 8 + c, 0});
                }
                for (int dc : {-1, 1})
                {
                    int nc = c + dc;
                    if (!onBoard(nr, nc)) continue;
                    int t = nr * 8 + nc;
                    if (sq[t] * me < 0) add(t);
                    else if (t == ep) out.push_back({i, t, 0});
                }
            }
            else if (type == KNIGHT || type == KING)
            {
                for (int k = 0; k < 8; k++)
                {
                    int nr = type == KNIGHT ? r + KR[k] : r + DR[k], nc = type == KNIGHT ? c + KC[k] : c + DC[k];
                    if (onBoard(nr, nc) && sq[nr * 8 + nc] * me <= 0) out.push_back({i, nr * 8 + nc, 0});
                }
                if (type == KING) castlingMoves(i, out);
            }
            else
            {
                int d0 = (type == BISHOP) ? 4 : 0, d1 = (type == ROOK) ? 4 : 8;
                for (int d = d0; d < d1; d++)
                {
                    int nr = r + DR[d], nc = c + DC[d];
                    while (onBoard(nr, nc))
                    {
                        int q = sq[nr * 8 + nc];
                        if (q * me > 0) break;
                        out.push_back({i, nr * 8 + nc, 0});
                        if (q != EMPTY) break;
                        nr += DR[d]; nc += DC[d];
                    }
                }
            }
        }
    }

    void castlingMoves(int from, vector<Move> &out) const
    {
        bool w = whiteToMove;
        int home = w ? 60 : 4;
        if (from != home || attacked(home, !w)) return;
        int ks = w ? WK : BK, qs = w ? WQ : BQ;
        if ((castle & ks) && sq[home + 1] == EMPTY && sq[home + 2] == EMPTY &&
            !attacked(home + 1, !w) && !attacked(home + 2, !w))
            out.push_back({home, home + 2, 0});
        if ((castle & qs) && sq[home - 1] == EMPTY && sq[home - 2] == EMPTY && sq[home - 3] == EMPTY &&
            !attacked(home - 1, !w) && !attacked(home - 2, !w))
            out.push_back({home, home - 2, 0});
    }

    // Apply a (pseudo-legal) move to this board.
    void make(const Move &m)
    {
        int p = sq[m.from], type = abs(p), me = p > 0 ? 1 : -1;
        if (type == PAWN && m.to == ep && sq[m.to] == EMPTY && m.from % 8 != m.to % 8)
            sq[(m.from / 8) * 8 + m.to % 8] = EMPTY; // en-passant capture
        sq[m.to] = m.promo ? me * m.promo : p;
        sq[m.from] = EMPTY;
        if (type == KING && abs(m.to - m.from) == 2) // castling: move the rook too
        {
            if (m.to > m.from) { sq[m.to - 1] = sq[m.to + 1]; sq[m.to + 1] = EMPTY; }
            else { sq[m.to + 1] = sq[m.to - 2]; sq[m.to - 2] = EMPTY; }
        }
        auto strip = [&](int s) {
            if (s == 60) castle &= ~(WK | WQ);
            if (s == 63) castle &= ~WK;
            if (s == 56) castle &= ~WQ;
            if (s == 4) castle &= ~(BK | BQ);
            if (s == 7) castle &= ~BK;
            if (s == 0) castle &= ~BQ;
        };
        strip(m.from); strip(m.to);
        ep = (type == PAWN && abs(m.to - m.from) == 16) ? (m.from + m.to) / 2 : -1;
        whiteToMove = !whiteToMove;
    }

    // Legal moves = pseudo-legal moves that don't leave our own king attacked.
    vector<Move> legalMoves() const
    {
        vector<Move> pseudo, legal;
        pseudoMoves(pseudo);
        for (const Move &m : pseudo)
        {
            Board c = *this;
            c.make(m);
            if (!c.attacked(c.kingSquare(whiteToMove), !whiteToMove)) legal.push_back(m);
        }
        return legal;
    }
};

// ===================== Evaluation (from side-to-move's perspective) =====================
int evaluate(const Board &b)
{
    int score = 0;
    for (int i = 0; i < 64; i++)
    {
        int p = b.sq[i];
        if (!p) continue;
        int t = abs(p), s = p > 0 ? 1 : -1, r = i / 8, c = i % 8;
        int v = VALUE[t];
        int advance = p > 0 ? 7 - r : r;
        int centrality = 6 - (abs(2 * c - 7) / 2 + abs(2 * r - 7) / 2); // 0..6
        if (t == PAWN) v += advance * 8 + centrality * 2;
        else if (t == KNIGHT) v += centrality * 5;
        else if (t == BISHOP) v += centrality * 3;
        score += s * v;
    }
    return b.whiteToMove ? score : -score;
}

// ===================== Search: negamax + alpha-beta =====================
long long nodes = 0;
bool usePruning = true;

void orderMoves(vector<Move> &moves, const Board &b) // MVV-LVA: try captures of big pieces by small pieces first
{
    auto key = [&](const Move &m) {
        int victim = abs(b.sq[m.to]), attacker = abs(b.sq[m.from]);
        int k = victim ? 10 * VALUE[victim] - VALUE[attacker] + 10000 : 0;
        if (m.promo) k += VALUE[m.promo];
        return k;
    };
    stable_sort(moves.begin(), moves.end(), [&](const Move &a, const Move &c) { return key(a) > key(c); });
}

int negamax(const Board &b, int depth, int alpha, int beta, int ply)
{
    nodes++;
    vector<Move> moves = b.legalMoves();
    if (moves.empty()) return b.inCheck() ? -MATE + ply : 0; // checkmate (prefer faster mates) or stalemate
    if (depth == 0) return evaluate(b);
    orderMoves(moves, b);
    int best = -INF;
    for (const Move &m : moves)
    {
        Board c = b;
        c.make(m);
        int score = -negamax(c, depth - 1, -beta, -alpha, ply + 1);
        best = max(best, score);
        if (usePruning)
        {
            alpha = max(alpha, score);
            if (alpha >= beta) break; // beta cutoff
        }
    }
    return best;
}

string moveToStr(const Move &m)
{
    string s = Board::toAlg(m.from) + Board::toAlg(m.to);
    if (m.promo) s += "pnbrqk"[m.promo - 1];
    return s;
}

bool searchRoot(const Board &b, int depth, Move &bestMove, int &bestScore)
{
    nodes = 0;
    vector<Move> moves = b.legalMoves();
    if (moves.empty()) return false;
    orderMoves(moves, b);
    int alpha = -INF, beta = INF;
    bestScore = -INF;
    for (const Move &m : moves)
    {
        Board c = b;
        c.make(m);
        int score = -negamax(c, depth - 1, -beta, -alpha, 1);
        if (score > bestScore) { bestScore = score; bestMove = m; }
        if (usePruning) alpha = max(alpha, score);
    }
    return true;
}

// ===================== Testing aid: perft =====================
long long perft(const Board &b, int d)
{
    vector<Move> ms = b.legalMoves();
    if (d == 1) return (long long)ms.size();
    long long n = 0;
    for (const Move &m : ms) { Board c = b; c.make(m); n += perft(c, d - 1); }
    return n;
}

// ===================== UCI =====================
bool applyUciMove(Board &b, const string &s)
{
    if (s.size() < 4) return false;
    int from = Board::fromAlg(s.substr(0, 2)), to = Board::fromAlg(s.substr(2, 2)), promo = 0;
    if (s.size() >= 5)
    {
        char ch = tolower(s[4]);
        promo = ch == 'n' ? KNIGHT : ch == 'b' ? BISHOP : ch == 'r' ? ROOK : ch == 'q' ? QUEEN : 0;
    }
    for (const Move &m : b.legalMoves()) // move validation: must be in the legal move list
        if (m.from == from && m.to == to && m.promo == promo) { b.make(m); return true; }
    return false;
}

void uciLoop()
{
    Board board;
    string line;
    while (getline(cin, line))
    {
        istringstream iss(line);
        vector<string> tok;
        for (string t; iss >> t;) tok.push_back(t);
        if (tok.empty()) continue;
        const string &cmd = tok[0];
        if (cmd == "uci")
            cout << "id name ChatChess\nid author Manav Rathi\nuciok" << endl;
        else if (cmd == "isready") cout << "readyok" << endl;
        else if (cmd == "ucinewgame") board.setFen(START_FEN);
        else if (cmd == "position")
        {
            size_t i = 1;
            if (i < tok.size() && tok[i] == "startpos") { board.setFen(START_FEN); i++; }
            else if (i < tok.size() && tok[i] == "fen")
            {
                string fen;
                for (i++; i < tok.size() && tok[i] != "moves"; i++) fen += tok[i] + " ";
                board.setFen(fen);
            }
            if (i < tok.size() && tok[i] == "moves")
                for (i++; i < tok.size(); i++)
                    if (!applyUciMove(board, tok[i])) break;
        }
        else if (cmd == "go")
        {
            int depth = 3, perftDepth = 0;
            usePruning = true;
            for (size_t i = 1; i < tok.size(); i++)
            {
                if (tok[i] == "depth" && i + 1 < tok.size()) depth = stoi(tok[++i]);
                else if (tok[i] == "perft" && i + 1 < tok.size()) perftDepth = stoi(tok[++i]);
                else if (tok[i] == "nopruning") usePruning = false;
            }
            if (perftDepth > 0)
                cout << "nodes " << perft(board, perftDepth) << endl;
            else
            {
                Move best; int score = 0;
                if (!searchRoot(board, depth, best, score)) { cout << "bestmove 0000" << endl; continue; }
                cout << "info depth " << depth << " score ";
                if (abs(score) > MATE - 100) cout << "mate " << (score > 0 ? 1 : -1) * ((MATE - abs(score) + 1) / 2);
                else cout << "cp " << score;
                cout << " nodes " << nodes << endl;
                cout << "bestmove " << moveToStr(best) << endl;
            }
        }
        else if (cmd == "quit") break;
    }
}

int main()
{
    uciLoop();
    return 0;
}
