#ifndef HCUBE_TICTACTOESHARED_H_INCLUDED
#define HCUBE_TICTACTOESHARED_H_INCLUDED

#ifndef DOXYGEN_PROCESSOR

#include "NEAT.h"

namespace HCUBE
{
    class TicTacToeStats : public NEAT::Stringable
    {
    public:
        int wins,losses,ties;

        TicTacToeStats()
                :
                wins(0),
                losses(0),
                ties(0)
        {}

        TicTacToeStats(
            int _wins,
            int _losses,
            int _ties
        )
                :
                wins(_wins),
                losses(_losses),
                ties(_ties)
        {}

        virtual ~TicTacToeStats()
        {}

        virtual string toString() const
        {
            std::ostringstream oss;
            oss << "W:" << wins << " T:" << ties << " L:" << losses;
            return oss.str();
        }

        void operator +=(const TicTacToeStats &other)
        {
            wins += other.wins;
            losses += other.losses;
            ties += other.ties;
        }

        virtual Stringable *clone() const
        {
            return new TicTacToeStats(wins,losses,ties);
        }
    };
}

#endif

#endif // HCUBE_TICTACTOESHARED_H_INCLUDED
