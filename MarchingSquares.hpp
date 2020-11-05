#include <vector>

//A couple of adapters to decouple grid generators and the output vertices.
class ISquaresGenerator
{
public:
    virtual double getPoint(size_t x, size_t y) = 0;
};

class ISquaresOutput
{
public:
    virtual void resetVertices(size_t vertexCount) = 0;
    virtual void addVertex(double isoLevel, double x, double y) = 0;

    [[maybe_unused]] virtual void setVertex(size_t vertexIndex, double x, double y) = 0;
};


template <size_t ResolutionX, size_t ResolutionY, size_t PixelsPerPointX, size_t PixelsPerPointY>
class MarchingSquares
{
    constexpr static size_t ArraySize = ResolutionX * ResolutionY;

    std::vector<double> mAllPoints;

    ISquaresGenerator &mGenerator;
    ISquaresOutput &mOutput;

    inline uint8_t getSquareType(const size_t x, const size_t y, const double isoLevel) //Convert a square's four corners to a 4-bit integer. BottomLeft,BottomRight,TopRight,TopLeft with TopLeft being LSB.
    {
        uint8_t squareType = 0;
        if(this->getPoint(x, y) > isoLevel)
            squareType |= 0x1u;
        if(this->getPoint(x+1, y) > isoLevel)
            squareType |= 0x2u;
        if(this->getPoint(x+1, y+1) > isoLevel)
            squareType |= 0x4u;
        if(this->getPoint(x, y+1) > isoLevel)
            squareType |= 0x8u;

        return squareType;
    }

    //simple helper function to find if floating point numbers are equal.
    template<typename T>
    inline bool isEqual(T a, T b)
    {
        return std::abs(a-b) < std::numeric_limits<T>::epsilon();
    }

    template<typename TupleType, size_t... element>
    [[maybe_unused]] TupleType tupleAdd(TupleType& a, const TupleType& b, std::integer_sequence<size_t, element...>)
    {
        return { (std::get<element>(a) += std::get<element>(b), 0)... };
    }
                                                                                    //left-0                    top-1                       right-2                     bottom-3
    [[maybe_unused]]                                                                //topLeft-4                 topRight-5                  bottomRight-6               bottomLeft-7
    constexpr static std::array<std::tuple<double, double>, 8> squareVerticies{     std::make_tuple(0.0, 0.0), std::make_tuple(0.0, 0.0), std::make_tuple(1.0, 0.0), std::make_tuple(0.0, 1.0),
                                                                                    std::make_tuple(0.0, 0.0), std::make_tuple(1.0, 0.0), std::make_tuple(1.0, 1.0), std::make_tuple(0.0, 1.0)};

    constexpr static std::array<std::array<int, 10>, 16> squareIndicies {{
                                                                            /*00*/{-1},
                                                                            /*01*/{4, 0, 1, -1},
                                                                            /*02*/{1, 2, 5, -1},
                                                                            /*03*/{4, 0, 5, 5, 0, 2, -1},
                                                                            /*04*/{2, 3, 6, -1},
                                                                            /*05*/{4, 0, 1, 2, 3, 6, -1},
                                                                            /*06*/{5, 1, 6, 6, 1, 3, -1},
                                                                            /*07*/{4, 0, 5, 5, 0, 3, 3, 6, 5, -1},
                                                                            /*08*/{0, 7, 3, -1},
                                                                            /*09*/{4, 7, 3, 3, 1, 4, -1},
                                                                            /*10*/{1, 2, 5, 0, 7, 3, -1},
                                                                            /*11*/{5, 4, 2, 2, 4, 3, 3, 4, 7, -1},
                                                                            /*12*/{2, 0, 7, 7, 6, 2, -1},
                                                                            /*13*/{1, 4, 7, 7, 2, 1, 2, 7, 6, -1},
                                                                            /*14*/{5, 1, 6, 6, 1, 0, 0, 7, 6, -1},
                                                                            /*15*/{4, 7, 5, 5, 7, 6, -1}

                                                                        }};




public:
    MarchingSquares(ISquaresGenerator &generator, ISquaresOutput &output) : mAllPoints(ArraySize, 0), mGenerator(generator), mOutput(output)
    {
        recalculate(); //Calculate the first frame
    }

    void recalculate()
    {
        for(size_t y = 0; y < ResolutionY; y++)
            for(size_t x = 0; x < ResolutionX; x++)
                mAllPoints[(y * ResolutionX) + x] = this->mGenerator.getPoint(x, y); //Use the generator to generate all the points in a frame
    }

    std::vector<double> &getAllPoints()
    {
        return mAllPoints;
    }

    //return a point at x/y
    constexpr inline double getPoint(const size_t x, const size_t y)
    {
        return mAllPoints[(y * ResolutionX) + x];
    }

    //count total lines in a frame before rendering. Saves on 1000s of memory/copy operations on the VertexArray.
    constexpr size_t countVerticies(const double contour)
    {
        size_t vertexCount = 0;
        for(size_t y = 0; y < ResolutionY-1; y++)
        {
            for(size_t x = 0; x < ResolutionX-1; x++)
            {
                uint8_t squareType = getSquareType(x, y, contour);
                if(squareType > 0 && squareType < 15)
                    vertexCount += (squareType == 5 || squareType == 10) ? 4 : 2;
            }
        }
        return vertexCount;
    }
    size_t render(const std::vector<double> isoLevels)
    {
        mOutput.resetVertices(0);
        size_t currentVertex = 0;

        std::array<std::tuple<double, double>, 8> interpolated  {
                                                                    std::make_tuple(0.0, 0.5),
                                                                    std::make_tuple(0.5, 0.0),
                                                                    std::make_tuple(0.0, 0.5),
                                                                    std::make_tuple(0.5, 0.0),
                                                                    std::make_tuple(0.0, 0.0),
                                                                    std::make_tuple(0.0, 0.0),
                                                                    std::make_tuple(0.0, 0.0),
                                                                    std::make_tuple(0.0, 0.0)
                                                                };

        for(const auto isoLevel : isoLevels)
        {
            for(size_t y = 0; y < ResolutionY-1; y++)
            {
                for(size_t x = 0; x < ResolutionX-1; x++)
                {
                    const uint8_t squareType = getSquareType(x, y, isoLevel);

                    std::get<1>(interpolated[0]) = PixelsPerPointY * std::abs((isoLevel - this->getPoint(x, y)) / (this->getPoint(x, y+1) - this->getPoint(x, y)));
                    std::get<0>(interpolated[1]) = PixelsPerPointX * std::abs((isoLevel - this->getPoint(x, y)) / (this->getPoint(x+1, y) - this->getPoint(x, y)));
                    std::get<1>(interpolated[2]) = PixelsPerPointY * std::abs((isoLevel - this->getPoint(x+1, y)) / (this->getPoint(x+1, y+1) - this->getPoint(x+1, y)));
                    std::get<0>(interpolated[3]) = PixelsPerPointX * std::abs((isoLevel - this->getPoint(x, y+1)) / (this->getPoint(x+1, y+1) - this->getPoint(x, y+1)));

                    for(int i : squareIndicies[squareType])
                    {
                        if(i == -1) break;
                        auto interTuple =   std::make_tuple(((std::get<0>(squareVerticies[i]) + static_cast<double>(x)) * PixelsPerPointX) + (std::get<0>(interpolated[i])),
                                                            ((std::get<1>(squareVerticies[i]) + static_cast<double>(y)) * PixelsPerPointY) + (std::get<1>(interpolated[i])) );

                        mOutput.addVertex(isoLevel, std::get<0>(interTuple), std::get<1>(interTuple));
                        currentVertex++;
                    }
                }
            }
        }
        return currentVertex;
    }
};
