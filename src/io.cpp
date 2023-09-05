#include "piece_data.h"
#include "misc.h"
#include "threads.h"
#include "movegen.h"
#include "io.h"
#include "uci.h"
#include <iostream>
#include <sstream>
#include <iomanip>

#define FR2SQ(rank, file) (64 - ((file << 3) | rank))

bool print_uci = false;
void PrintBitboard(const Bitboard bitboard) {
    // print offset
    std::cout << std::endl;

    // loop over board ranks
    for (int rank = 0; rank < 8; rank++) {
        // loop over board files
        for (int file = 0; file < 8; file++) {
            // convert file & rank into square index
            int square = (rank * 8 + file);

            // print ranks
            if (!file)
                printf("  %d ", 8 - rank);

            // print bit state (either 1 or 0)
            printf(" %d", get_bit(bitboard, square) ? 1 : 0);
        }

        // print new line every rank
        std::cout << std::endl;
    }

    // print board files
    printf("\n     a b c d e f g h\n\n");

    // print bitboard as unsigned decimal number
    std::cout << "     Bitboard: " << bitboard << "\n\n";
}

// print board
void PrintBoard(const S_Board* pos) {
    // print offset
    std::cout << "\n";

    // loop over board ranks
    for (int rank = 0; rank < 8; rank++) {
        // loop ober board files
        for (int file = 0; file < 8; file++) {
            // init square
            int square = rank * 8 + file;

            // print ranks
            if (!file)
                printf("  %d ", 8 - rank);

            // define piece variable
            int piece = -1;

            // loop over all piece pos->bitboards
            for (int bb_piece = WP; bb_piece <= BK; bb_piece++) {
                // if there is a piece on current square
                if (get_bit(pos->bitboards[bb_piece], square))
                    // get piece code
                    piece = bb_piece;
            }

            printf(" %c", (piece == -1) ? '.' : ascii_pieces[piece]);
        }

        // print new line every rank
        std::cout << "\n";
    }

    // print board files
    printf("\n     a b c d e f g h\n\n");

    // print side to move
    printf("     Side:     %s\n", !pos->side ? "white" : "black");

    // print enpassant square
    printf("     Enpassant:   %s\n",
        (GetEpSquare(pos) != no_sq) ? square_to_coordinates[GetEpSquare(pos)] : "no");

    // print castling rights
    printf("     Castling:  %c%c%c%c\n", (pos->GetCastlingPerm() & WKCA) ? 'K' : '-',
        (pos->GetCastlingPerm() & WQCA) ? 'Q' : '-',
        (pos->GetCastlingPerm() & BKCA) ? 'k' : '-',
        (pos->GetCastlingPerm() & BQCA) ? 'q' : '-');

    std::cout << "position hisPly: " << pos->hisPly << std::endl;

    std::cout << "position key: " << pos->posKey << std::endl;

    std::cout << "Fen: " << GetFen(pos) << "\n\n";
}

// print attacked squares
void PrintAttackedSquares(const S_Board* pos, const int side) {
    std::cout << "\n";

    // loop over board ranks
    for (int rank = 0; rank < 8; rank++) {
        // loop over board files
        for (int file = 0; file < 8; file++) {
            // init square
            int square = rank * 8 + file;

            // print ranks
            if (!file)
                printf("  %d ", 8 - rank);

            // check whether current square is attacked or not
            printf(" %d", IsSquareAttacked(pos, square, side) ? 1 : 0);
        }

        // print new line every rank
        std::cout << std::endl;
    }

    // print files
    std::cout << "\n     a b c d e f g h\n\n";
}

// print move (for UCI purposes)
void PrintMove(const int move) {
    const char* from = square_to_coordinates[From(move)];
    const char* to = square_to_coordinates[To(move)];

    if (isPromo(move))
        std::cout << from << to << promoted_pieces[getPromotedPiecetype(move)];
    else
        std::cout << from << to;
}

char* FormatMove(const int move) {
    static char Move[6];
    const char* from = square_to_coordinates[From(move)];
    const char* to = square_to_coordinates[To(move)];

    if (isPromo(move))
        snprintf(Move, sizeof(Move), "%s%s%c", from, to, promoted_pieces[getPromotedPiecetype(move)]);
    else
        snprintf(Move, sizeof(Move), "%s%s", from, to);

    return Move;
}

void PrintMoveList(const S_MOVELIST* list) {
    for (int index = 0; index < list->count; ++index) {
        int move = list->moves[index].move;
        int score = list->moves[index].score;
        std::cout << "Move: " << index + 1 << " >" << FormatMove(move) << " score: " << score << std::endl;
    }
    std::cout << "MoveList Total  Moves:" << "list->count" << "\n\n";
}

std::string Pick_color(int score) {
    // drawish score, no highlight
    if (abs(score) <= 10) return "\033[38;5;7m";
    // Good mate score, blue
    if (score > mate_found) return "\033[38;5;39m";
    // positive for us, light green
    if (score > 10) return "\033[38;5;42m";
    // negative for us, red
    if (score < 10) return "\033[38;5;9m";
    return "\033[0m";
}

// Prints the uci output
void PrintUciOutput(const int score, const int depth, const S_ThreadData* td, const S_UciOptions* options) {
    // This handles the basic console output
    long time = GetTimeMs() - td->info.starttime;
    uint64_t nodes = td->info.nodes + GetTotalNodes();

    uint64_t nps = nodes / (time + !time) * 1000;
    if (print_uci) {
        if (score > -mate_score && score < -mate_found)
            std::cout << "info score mate " << -(score + mate_score) / 2 << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
            " nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

        else if (score > mate_found && score < mate_score)
            std::cout << "info score mate " << (mate_score - score) / 2 + 1 << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
            " nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

        else
            std::cout << "info score cp " << score << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
            " nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

        // loop over the moves within a PV line
        for (int count = 0; count < td->pv_table.pvLength[0]; count++) {
            // print PV move
            PrintMove(td->pv_table.pvArray[0][count]);
            printf(" ");
        }

        // print new line
        std::cout << std::endl;
    }
    else {
        // Convert time in seconds if possible
        std::string time_unit = "ms";
        float parsed_time = time;
        if (parsed_time >= 1000) {
            parsed_time = parsed_time / 1000;
            time_unit = 's';
            if (parsed_time >= 60) {
                parsed_time = parsed_time / 60;
                time_unit = 'm';
            }
        }

        // convert time to string
        std::stringstream time_stream;
        time_stream << std::setprecision(3) << parsed_time;
        std::string time_string = time_stream.str() + time_unit;

        // Convert score to a decimal format or to a mate string
        int score_precision = 0;
        float parsed_score = 0;
        std::string score_unit;
        if (score > -mate_score && score < -mate_found) {
            parsed_score = std::abs((score + mate_score) / 2);
            score_unit = "-M";
        }
        else if (score > mate_found && score < mate_score) {
            parsed_score = (mate_score - score) / 2 + 1;
            score_unit = "+M";
        }
        else {
            parsed_score = static_cast<float>(score) / 100;
            if (parsed_score >= 0) score_unit = '+';
            score_precision = 2;
        }
        // convert score to string
        std::stringstream score_stream;
        score_stream << std::fixed << std::setprecision(score_precision) << parsed_score;
        std::string score_color = Pick_color(score);
        std::string color_reset = "\033[0m";
        std::string score_string = score_color + score_unit + score_stream.str() + color_reset;
        // convert nodes into string
        int node_precision = 0;
        std::string node_unit = "n";
        float parsed_nodes = static_cast<float>(nodes);
        if (parsed_nodes >= 1000) {
            parsed_nodes = parsed_nodes / 1000;
            node_unit = "Kn";
            node_precision = 2;
            if (parsed_nodes >= 1000) {
                parsed_nodes = parsed_nodes / 1000;
                node_unit = "Mn";
            }
            if (parsed_nodes >= 1000) {
                parsed_nodes = parsed_nodes / 1000;
                node_unit = "Gn";
            }
        }

        std::stringstream node_stream;
        node_stream << std::fixed << std::setprecision(node_precision) << parsed_nodes;
        std::string node_string = node_stream.str() + node_unit;

        // Pretty print search info
        std::cout << std::setw(3) << depth << "/";
        std::cout << std::left << std::setw(3) << td->info.seldepth;

        std::cout << std::right << std::setw(8) << time_string;
        std::cout << std::right << std::setw(10) << node_string;
        std::cout << std::setw(7) << std::right << " " << score_string;
        std::cout << std::setw(7) << std::right << std::fixed << static_cast<int>(nps / 1000.0) << "kn/s" << " ";

        // loop over the moves within a PV line
        for (int count = 0; count < td->pv_table.pvLength[0]; count++) {
            // print PV move
            PrintMove(td->pv_table.pvArray[0][count]);
            std::cout << " ";
        }

        // print new line
        std::cout << "\n";
    }
}
