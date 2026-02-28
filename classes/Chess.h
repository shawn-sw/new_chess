#pragma once

#include "Game.h"
#include "Grid.h"

constexpr int pieceSize = 80;

enum ChessPiece
{
    NoPiece,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King
};

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;
    void bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    void pieceTaken(Bit *bit) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;
    ChessPiece pieceTypeForBit(const Bit& bit) const;
    bool isPathClear(int srcX, int srcY, int dstX, int dstY) const;
    bool canPawnMove(const Bit& bit, const ChessSquare& src, const ChessSquare& dst) const;
    bool canCastle(const Bit& bit, const ChessSquare& src, const ChessSquare& dst) const;
    bool isSquareUnderAttack(int x, int y, int attackingPlayer) const;
    bool pieceAttacksSquare(const Bit& bit, const ChessSquare& src, int targetX, int targetY) const;
    void markCastlingStateForMove(const Bit& bit, const ChessSquare& src);
    void markCastlingStateForCapturedPiece(const Bit& bit, const ChessSquare& square);

    Grid* _grid;
    bool _kingMoved[2];
    bool _rookMoved[2][2];
    int _enPassantTargetX;
    int _enPassantTargetY;
};
