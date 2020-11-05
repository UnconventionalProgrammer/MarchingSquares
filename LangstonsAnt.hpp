#include <vector>
#include <tuple>

template <size_t Width, size_t Height, class CellType = double>
class LangstonsAnt
{
    constexpr static size_t ArraySize = Width * Height;
    std::vector<CellType> mCells;
    std::tuple<size_t, size_t, uint8_t> mAnt;

    inline const size_t pointToArray(const std::tuple<size_t, size_t> &point)
    {
        return (std::get<1>(point) * Width) + std::get<0>(point);
    }

    inline void flipCellState(const std::tuple<size_t, size_t> &point)
    {
        mCells[pointToArray(point)] = mCells[pointToArray(point)] ? 0 : 1;
    }

public:
    LangstonsAnt() : mCells(ArraySize, 0), mAnt(std::make_tuple(Width/2, Height/2, 0)) {}

    /***************************************
    At a white square, turn 90° clockwise, flip the color of the square, move forward one unit
    At a black square, turn 90° counter-clockwise, flip the color of the square, move forward one unit
    ****************************************/
    void update()
    {
        auto &[x, y, r] = mAnt;
        flipCellState(std::make_tuple(x, y));
        CellType state = getCellState(std::make_tuple(x, y));

        if(state > 0)
        {
            ++r;
            r = r > 3 ? 0 : r;
        }
        else
        {
            --r;
            r = r > 3 ? 3 : r;
        }

        switch(r)
        {
        case 0:
            --y;
            if(y > Height) y = Height-1;
            break;
        case 1:
            ++x;
            if(x > Width) x = 0;
            break;
        case 2:
            ++y;
            if(y > Height) y = 0;
            break;
        case 3:
            --x;
            if(x > Width) x = Width-1;
            break;
        }
    }

    const std::vector<CellType> &getCells()
    {
        return mCells;
    }

    inline const CellType getCellState(const std::tuple<size_t, size_t> &point)
    {
        return mCells[pointToArray(point)];
    }


};
