#include "Chess.h"
#include <limits>
#include <cmath>
#include <sstream>
#include <cctype>

Chess::Chess()
{
    _grid = new Grid(8, 8);
    _kingMoved[0] = _kingMoved[1] = false;
    _rookMoved[0][0] = _rookMoved[0][1] = false;
    _rookMoved[1][0] = _rookMoved[1][1] = false;
    _enPassantTargetX = -1;
    _enPassantTargetY = -1;
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0pnbrqk" };
    const char *bpieces = { "0PNBRQK" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setGameTag((playerNumber == 0 ? 0 : 128) + static_cast<int>(piece));
    bit->setSize(pieceSize, pieceSize);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;
    _kingMoved[0] = _kingMoved[1] = false;
    _rookMoved[0][0] = _rookMoved[0][1] = false;
    _rookMoved[1][0] = _rookMoved[1][1] = false;
    _enPassantTargetX = -1;
    _enPassantTargetY = -1;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    

    startGame();
}

void Chess::FENtoBoard(const std::string& fen) {
    // Accept either board-only FEN or full FEN by using the first space-delimited field.
    std::istringstream fenStream(fen);
    std::string placement;
    fenStream >> placement;
    if (placement.empty()) {
        return;
    }

    // Clear any existing pieces first.
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });

    int x = 0;
    int y = 7; // FEN starts at rank 8, while our board uses y=0 for rank 1.

    for (char c : placement) {
        if (c == '/') {
            if (x != 8) {
                return;
            }
            x = 0;
            --y;
            if (y < 0) {
                return;
            }
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            x += c - '0';
            if (x > 8) {
                return;
            }
            continue;
        }

        ChessPiece piece = NoPiece;
        switch (static_cast<char>(std::tolower(static_cast<unsigned char>(c)))) {
            case 'p': piece = Pawn; break;
            case 'n': piece = Knight; break;
            case 'b': piece = Bishop; break;
            case 'r': piece = Rook; break;
            case 'q': piece = Queen; break;
            case 'k': piece = King; break;
            default: return;
        }

        if (x >= 8 || y < 0 || y > 7) {
            return;
        }

        const int playerNumber = std::isupper(static_cast<unsigned char>(c)) ? 0 : 1;
        Bit* bit = PieceForPlayer(playerNumber, piece);
        ChessSquare* square = _grid->getSquare(x, y);
        bit->setPosition(square->getPosition());
        square->setBit(bit);
        ++x;
    }

    if (x != 8 || y != 0) {
        return;
    }
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor == currentPlayer) return true;
    return false;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* srcSquare = static_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = static_cast<ChessSquare*>(&dst);

    if (!srcSquare || !dstSquare) {
        return false;
    }

    const int srcX = srcSquare->getColumn();
    const int srcY = srcSquare->getRow();
    const int dstX = dstSquare->getColumn();
    const int dstY = dstSquare->getRow();

    if (srcX == dstX && srcY == dstY) {
        return false;
    }

    const int dx = dstX - srcX;
    const int dy = dstY - srcY;
    const int absDx = std::abs(dx);
    const int absDy = std::abs(dy);
    const ChessPiece piece = pieceTypeForBit(bit);

    switch (piece) {
        case Pawn:
            return canPawnMove(bit, *srcSquare, *dstSquare);
        case Knight:
            return (absDx == 1 && absDy == 2) || (absDx == 2 && absDy == 1);
        case Bishop:
            return absDx == absDy && isPathClear(srcX, srcY, dstX, dstY);
        case Rook:
            return (dx == 0 || dy == 0) && isPathClear(srcX, srcY, dstX, dstY);
        case Queen:
            return ((dx == 0 || dy == 0) || (absDx == absDy)) && isPathClear(srcX, srcY, dstX, dstY);
        case King:
            if (absDx <= 1 && absDy <= 1) {
                return true;
            }
            return canCastle(bit, *srcSquare, *dstSquare);
        case NoPiece:
        default:
            return false;
    }
}

void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* srcSquare = static_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = static_cast<ChessSquare*>(&dst);
    if (!srcSquare || !dstSquare) {
        endTurn();
        return;
    }

    const ChessPiece piece = pieceTypeForBit(bit);
    const int srcX = srcSquare->getColumn();
    const int srcY = srcSquare->getRow();
    const int dstX = dstSquare->getColumn();
    const int dstY = dstSquare->getRow();
    const int previousEnPassantX = _enPassantTargetX;
    const int previousEnPassantY = _enPassantTargetY;

    markCastlingStateForMove(bit, *srcSquare);
    _enPassantTargetX = -1;
    _enPassantTargetY = -1;

    if (piece == King && std::abs(dstX - srcX) == 2 && dstY == srcY) {
        const bool kingSide = dstX > srcX;
        ChessSquare* rookSource = _grid->getSquare(kingSide ? 7 : 0, srcY);
        ChessSquare* rookDestination = _grid->getSquare(kingSide ? dstX - 1 : dstX + 1, srcY);
        if (rookSource && rookDestination && rookSource->bit()) {
            Bit* rook = rookSource->bit();
            rookDestination->setBit(rook);
            rook->setParent(rookDestination);
            rook->setPosition(rookDestination->getPosition());
            rookSource->setBit(nullptr);
        }
    }

    if (piece == Pawn) {
        const int direction = bit.getOwner()->playerNumber() == 0 ? 1 : -1;

        if (srcX != dstX && previousEnPassantX == dstX && previousEnPassantY == dstY) {
            ChessSquare* capturedPawnSquare = _grid->getSquare(dstX, dstY - direction);
            if (capturedPawnSquare) {
                capturedPawnSquare->destroyBit();
            }
        }

        if (std::abs(dstY - srcY) == 2) {
            _enPassantTargetX = srcX;
            _enPassantTargetY = srcY + direction;
        }

        const int promotionRank = bit.getOwner()->playerNumber() == 0 ? 7 : 0;
        if (dstY == promotionRank) {
            const int playerNumber = bit.getOwner()->playerNumber();
            dstSquare->destroyBit();
            Bit* promotedBit = PieceForPlayer(playerNumber, Queen);
            promotedBit->setPosition(dstSquare->getPosition());
            dstSquare->setBit(promotedBit);
        }
    }

    endTurn();
}

void Chess::stopGame()
{
    _enPassantTargetX = -1;
    _enPassantTargetY = -1;
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
            s += pieceNotation( x, y );
        }
    );
    return s;}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}

ChessPiece Chess::pieceTypeForBit(const Bit& bit) const
{
    const int encodedPiece = bit.gameTag() & 127;
    if (encodedPiece < Pawn || encodedPiece > King) {
        return NoPiece;
    }
    return static_cast<ChessPiece>(encodedPiece);
}

bool Chess::isPathClear(int srcX, int srcY, int dstX, int dstY) const
{
    const int stepX = (dstX > srcX) ? 1 : (dstX < srcX ? -1 : 0);
    const int stepY = (dstY > srcY) ? 1 : (dstY < srcY ? -1 : 0);

    int x = srcX + stepX;
    int y = srcY + stepY;
    while (x != dstX || y != dstY) {
        ChessSquare* square = _grid->getSquare(x, y);
        if (!square) {
            return false;
        }
        if (square->bit()) {
            return false;
        }
        x += stepX;
        y += stepY;
    }

    return true;
}

bool Chess::canPawnMove(const Bit& bit, const ChessSquare& src, const ChessSquare& dst) const
{
    const int srcX = src.getColumn();
    const int srcY = src.getRow();
    const int dstX = dst.getColumn();
    const int dstY = dst.getRow();
    const int dx = dstX - srcX;
    const int dy = dstY - srcY;
    const int direction = bit.getOwner()->playerNumber() == 0 ? 1 : -1;
    const int startRow = bit.getOwner()->playerNumber() == 0 ? 1 : 6;
    Bit* targetBit = _grid->getSquare(dstX, dstY)->bit();

    if (dx == 0) {
        if (targetBit) {
            return false;
        }
        if (dy == direction) {
            return true;
        }
        if (dy == direction * 2 && srcY == startRow) {
            ChessSquare* middleSquare = _grid->getSquare(srcX, srcY + direction);
            return middleSquare && middleSquare->bit() == nullptr;
        }
        return false;
    }

    if (std::abs(dx) == 1 && dy == direction) {
        if (targetBit != nullptr) {
            return targetBit->getOwner() != bit.getOwner();
        }
        return dstX == _enPassantTargetX && dstY == _enPassantTargetY;
    }

    return false;
}

bool Chess::canCastle(const Bit& bit, const ChessSquare& src, const ChessSquare& dst) const
{
    if (pieceTypeForBit(bit) != King) {
        return false;
    }

    const int playerNumber = bit.getOwner()->playerNumber();
    if (_kingMoved[playerNumber]) {
        return false;
    }

    const int srcX = src.getColumn();
    const int srcY = src.getRow();
    const int dstX = dst.getColumn();
    const int dstY = dst.getRow();
    const int dx = dstX - srcX;

    if (srcY != dstY || std::abs(dx) != 2) {
        return false;
    }

    const bool kingSide = dx > 0;
    if (_rookMoved[playerNumber][kingSide ? 1 : 0]) {
        return false;
    }

    ChessSquare* rookSquare = _grid->getSquare(kingSide ? 7 : 0, srcY);
    if (!rookSquare || !rookSquare->bit()) {
        return false;
    }

    Bit* rook = rookSquare->bit();
    if (rook->getOwner() != bit.getOwner() || pieceTypeForBit(*rook) != Rook) {
        return false;
    }

    const int rookX = rookSquare->getColumn();
    const int step = kingSide ? 1 : -1;
    for (int x = srcX + step; x != rookX; x += step) {
        ChessSquare* square = _grid->getSquare(x, srcY);
        if (!square) {
            return false;
        }
        if (square->bit()) {
            return false;
        }
    }

    const int opponent = 1 - playerNumber;
    if (isSquareUnderAttack(srcX, srcY, opponent)) {
        return false;
    }
    if (isSquareUnderAttack(srcX + step, srcY, opponent)) {
        return false;
    }
    if (isSquareUnderAttack(dstX, dstY, opponent)) {
        return false;
    }

    return true;
}

bool Chess::isSquareUnderAttack(int x, int y, int attackingPlayer) const
{
    for (int boardY = 0; boardY < 8; ++boardY) {
        for (int boardX = 0; boardX < 8; ++boardX) {
            ChessSquare* square = _grid->getSquare(boardX, boardY);
            if (!square || !square->bit()) {
                continue;
            }

            Bit* piece = square->bit();
            if (piece->getOwner()->playerNumber() != attackingPlayer) {
                continue;
            }

            if (pieceAttacksSquare(*piece, *square, x, y)) {
                return true;
            }
        }
    }

    return false;
}

bool Chess::pieceAttacksSquare(const Bit& bit, const ChessSquare& src, int targetX, int targetY) const
{
    const int srcX = src.getColumn();
    const int srcY = src.getRow();
    const int dx = targetX - srcX;
    const int dy = targetY - srcY;
    const int absDx = std::abs(dx);
    const int absDy = std::abs(dy);

    switch (pieceTypeForBit(bit)) {
        case Pawn: {
            const int direction = bit.getOwner()->playerNumber() == 0 ? 1 : -1;
            return absDx == 1 && dy == direction;
        }
        case Knight:
            return (absDx == 1 && absDy == 2) || (absDx == 2 && absDy == 1);
        case Bishop:
            return absDx == absDy && isPathClear(srcX, srcY, targetX, targetY);
        case Rook:
            return (dx == 0 || dy == 0) && isPathClear(srcX, srcY, targetX, targetY);
        case Queen:
            return ((dx == 0 || dy == 0) || (absDx == absDy)) && isPathClear(srcX, srcY, targetX, targetY);
        case King:
            return absDx <= 1 && absDy <= 1 && (absDx != 0 || absDy != 0);
        case NoPiece:
        default:
            return false;
    }
}

void Chess::markCastlingStateForMove(const Bit& bit, const ChessSquare& src)
{
    const int playerNumber = bit.getOwner()->playerNumber();
    const ChessPiece piece = pieceTypeForBit(bit);

    if (piece == King) {
        _kingMoved[playerNumber] = true;
        return;
    }

    if (piece != Rook) {
        return;
    }

    if (src.getRow() == (playerNumber == 0 ? 0 : 7)) {
        if (src.getColumn() == 0) {
            _rookMoved[playerNumber][0] = true;
        } else if (src.getColumn() == 7) {
            _rookMoved[playerNumber][1] = true;
        }
    }
}

void Chess::markCastlingStateForCapturedPiece(const Bit& bit, const ChessSquare& square)
{
    if (pieceTypeForBit(bit) != Rook) {
        return;
    }

    const int playerNumber = bit.getOwner()->playerNumber();
    if (square.getRow() == (playerNumber == 0 ? 0 : 7)) {
        if (square.getColumn() == 0) {
            _rookMoved[playerNumber][0] = true;
        } else if (square.getColumn() == 7) {
            _rookMoved[playerNumber][1] = true;
        }
    }
}

void Chess::pieceTaken(Bit *bit)
{
    if (!bit) {
        return;
    }

    ChessSquare* square = static_cast<ChessSquare*>(bit->getHolder());
    if (!square) {
        return;
    }

    markCastlingStateForCapturedPiece(*bit, *square);
}
