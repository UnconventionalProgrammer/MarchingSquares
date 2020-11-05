#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <future>
#include "SFML/Graphics.hpp"

//https://github.com/Reputeless/PerlinNoise
#include "PerlinNoise.hpp"
#include "MarchingSquares.hpp"

class [[maybe_unused]] PerlinHeightmapGenerator : public ISquaresGenerator
{
    const siv::PerlinNoise mPerlin;     //Noise generator
    double mOffsetX, mOffsetY, mOffsetZ;//Noise offsets
    size_t mResolutionX, mResolutionY;  //Total points x/y probably not the best variable names for this, but whatever.
public:
    //Init the seed the perlin generator and initialize member variables.
    PerlinHeightmapGenerator(size_t resolutionX, size_t resolutionY, size_t seed) : mPerlin(seed), mOffsetX(0.0), mOffsetY(0.0), mOffsetZ(1.0), mResolutionX(resolutionX), mResolutionY(resolutionY)
    {}

    double getPoint(size_t x, size_t y) override//Get the perlin noise for a point
    {
        return mPerlin.accumulatedOctaveNoise3D_0_1(static_cast<double>(x / (mResolutionX / 2.0)) + mOffsetX,
                                                    static_cast<double>(y / (mResolutionY / 2.0)) + mOffsetY,
                                                    mOffsetZ, 4);
    }

    //step is a nice helper function to have so swapping out point generators is a bit easier.
    void step(const double delta)
    {
        mOffsetZ += delta;
    }

    void setOffsets(double x, double y, double z) //Set the offset the perlin landscape position
    {
        mOffsetX = x;
        mOffsetY = y;
        mOffsetZ = z;
    }

    void moveOffsets(double x, double y, double z) //Move the offset the perlin landscape position
    {
        mOffsetX += x;
        mOffsetY += y;
        mOffsetZ += z;
    }

    void setResolution(size_t x, size_t y)  //Change the resolution of the perlin noise (higher numbers will "zoom in")
    {
        mResolutionX = x;
        mResolutionY = y;
    }
};

class MetaBallsGenerator : public ISquaresGenerator
{
    size_t mResolutionX, mResolutionY;
    //Yes, I'm using a tuple instead of a well defined struct...because f-you that's why.
    //an array of 5 doubles would also work.
    //CenterX CenterY VelocityX, VelocityY Radius
    std::vector<std::tuple<double, double, double, double, double>> mMetaBalls;

    void updatePositions(double delta)
    {
        for(auto &metaball : mMetaBalls)
        {
            auto &[posX, posY, velX, velY, radius] = metaball; //What an awesome way to tie variables to a tuple.

            posX += (velX * delta);
            posY += (velY * delta);

            //I hate this, but couldn't be bothered implementing it properly.
            //It essentially rotates the direction vector 90 degrees on collision with the boundary.
            //Maybe it would be cool to implement ball collisions
            if((posX - radius) < 0.0 || (posX + radius) > mResolutionX)
                velX = -velX;
            if((posY - radius) < 0.0 || (posY + radius) > mResolutionY)
                velY = -velY;
        }

    }

public:
    MetaBallsGenerator(size_t resolutionX, size_t resolutionY, size_t seed) : mResolutionX(resolutionX), mResolutionY(resolutionY)
    {
        //Random balls with random sizes with random directions
        std::default_random_engine generator(seed);
        std::uniform_int_distribution<int> ballCountDist(2, 10);
        std::uniform_real_distribution<double> radiusDist(2, resolutionX * 0.15); //Based on percentage of total points so sizes are consistent.
                                                                                  //(kind of, since it's not relative to window size).
        for(int i = ballCountDist(generator); i > 0; i--)
        {
            double radius = radiusDist(generator);

            std::uniform_real_distribution<double> posXDist(radius + 1.0, resolutionX - radius - 1.0); //At least try not to spawn balls in the walls.
            std::uniform_real_distribution<double> posYDist(radius + 1.0, resolutionY - radius - 1.0);
            std::uniform_real_distribution<double> speedDistX(-(resolutionX * 2.0), resolutionX * 2.0); //Not too fast, based on a percentage of total points
            std::uniform_real_distribution<double> speedDistY(-(resolutionY * 2.0), resolutionY * 2.0); //so speed is always consistent. (kind of, since it's not
                                                                                                        //relative to window size).

             mMetaBalls.emplace_back(std::make_tuple(posXDist(generator), posYDist(generator), speedDistX(generator), speedDistY(generator), radius));
        }
    }

    double getPoint(size_t x, size_t y) override
    {
        double ret = 0.0;
        for(auto &metaball : mMetaBalls)
        {
            auto &[posX, posY, velX, velY, radius] = metaball;

            //define our circle in a way that as the distance from the center increases, the returned value is smaller.
            //This makes the "blobiness" effect instead of well defined boundaries.
            ret += ((radius*radius) / (((static_cast<double>(x) - posX) * (static_cast<double>(x) - posX)) + ((static_cast<double>(y) - posY) * (static_cast<double>(y) - posY)))) * 0.3;

        }
        return ret;
    }

    void step(double delta)
    {
        //This helps with multi-threading
        //as each thread jumps forward a few steps it will run the simulation forward to the current thread's step.
        while(delta > 0.0001)
        {
            updatePositions(0.0001);
            delta -= 0.0001;
        }
        if(delta > 0.0)
            updatePositions(delta);
    }
};

//this just helped me with implementing the interpolation algorithms.
class TestPattern : public ISquaresGenerator
{
    constexpr static size_t TestPatternWidth = 5;
    constexpr static size_t TestPatternHeight = 5;
    constexpr static const std::array<double, 25> mTestPattern{ 0.0, 0.1, 0.1, 0.3, 0.2,
                                                                0.1, 0.3, 0.6, 0.6, 0.3,
                                                                0.3, 0.7, 0.9, 0.7, 0.3,
                                                                0.2, 0.7, 0.8, 0.6, 0.2,
                                                                0.1, 0.2, 0.3, 0.4, 0.7};
public:
    double getPoint(size_t x, size_t y) override
    {
        return mTestPattern[(y * TestPatternWidth) + x];
    }
};

//Convert the MarchingSquares output to something SFML can use.
class SFMLMarchingSquaresOutput : public ISquaresOutput
{
    sf::VertexArray mVertices;

public:
    SFMLMarchingSquaresOutput() : mVertices(sf::PrimitiveType::Triangles) {}
    void resetVertices(size_t vertexCount) override //clear vertex data and set the size of the buffer, this avoids 1000s of memory allocations
    {
        mVertices.clear();
        mVertices.resize(vertexCount);
    }

    void addVertex(double isoLevel, double x, double y)  override//Very slow, reallocates the buffer and copies the old one to the new one.
    {
        auto color = sf::Color::White;
        if(isoLevel < 0.5) color = sf::Color::Green;
        if(isoLevel < 0.4) color = sf::Color::Red;

        mVertices.append({{static_cast<float>(x), static_cast<float>(y)}, color});
    }

    void setVertex(size_t vertexIndex, double x, double y) override //Very fast, simply assign vertex data to an already allocated slot in the buffer.
    {
        mVertices[vertexIndex] = {{static_cast<float>(x), static_cast<float>(y)}, sf::Color::White};
    }

    const sf::VertexArray &getVertices() //Get all the vertex data for drawing
    {
        return this->mVertices;
    }
};

//Holds info to help with multi-threading
struct WorkerThread
{
    explicit WorkerThread() : threadId(0), isRunning(true) {}

    std::future<void> result;
    size_t threadId = 0;        //tells the thread where in the queue it is so it can offset it's output accordingly.
    std::atomic<bool> invalidated; //tells the thread to render the next frame.
    bool isRunning; //Flag tells the thread when to exit it's busy loop
    SFMLMarchingSquaresOutput output; //Each thread gets it's own VertexBuffer.
    std::mutex threadSync; //Synchronize access to output.
};

int main()
{
    constexpr size_t          PointsX = 200; //How many points across the X/Y axis
    constexpr size_t          PointsY = 200;
    constexpr size_t          PixelsPerPointX = 4; //Resolution of the display, higher numbers means the points a further apart.
    constexpr size_t          PixelsPerPointY = 4;
    constexpr double          DepthIncrementAmountPerFrame = 0.0005; //We are using 3d perlin noise, how fast should we "travel" through the Z axis.
    constexpr size_t          ThreadCount = 6;  //How many worker threads should we use. Probably better to use std::thread::hardware_concurrency()
    const std::vector<double> IsoLevels{0.3,0.4,0.5};   //Threshold for a 1 or 0 on points of the square.

    sf::RenderWindow window(sf::VideoMode(PointsX * PixelsPerPointX, PointsY * PixelsPerPointY), "Marching Squares Example");
    size_t seed = std::chrono::steady_clock::now().time_since_epoch().count();
    /**********************************************************************************************************************
                                            calculationThread Lambda
                            Generate the vertex data required asynchronously
    **********************************************************************************************************************/
    std::array<WorkerThread, ThreadCount> workerThreads;
    auto calculationThread = [seed, &IsoLevels, &workerThreads](const size_t threadId)
    {
        PerlinHeightmapGenerator generator(PointsX, PointsY, seed);
        //MetaBallsGenerator generator(PointsX, PointsY, seed);

        MarchingSquares<PointsX, PointsY, PixelsPerPointX, PixelsPerPointY> squares(generator, workerThreads[threadId].output);

        //Start off at a depth relative to the thread's position in the queue and depth increment amount
        generator.step(DepthIncrementAmountPerFrame * workerThreads[threadId].threadId);
        while(workerThreads[threadId].isRunning)
        {
            if(!workerThreads[threadId].invalidated) //Wait until the data has been used
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            workerThreads[threadId].threadSync.lock();
            squares.recalculate(); //calculate point data
            squares.render(IsoLevels);       //render the point data into vertices this is where the squares "march".

            generator.step(DepthIncrementAmountPerFrame * ThreadCount);
            workerThreads[threadId].invalidated = false; //new data has rendered and therefore valid for rendering again.
            workerThreads[threadId].threadSync.unlock();

        }
    };

    for(size_t threadIdCounter = 0; threadIdCounter < ThreadCount; threadIdCounter++)
    {
        workerThreads[threadIdCounter].result = std::move(std::async(std::launch::async, calculationThread, threadIdCounter));
        workerThreads[threadIdCounter].threadId = threadIdCounter;
        workerThreads[threadIdCounter].isRunning = true;
        workerThreads[threadIdCounter].invalidated = true;
    }

    sf::Font myFont;
    myFont.loadFromFile("Commodore.TTF");

    sf::Text frameTimerText;
    frameTimerText.setFont(myFont);
    frameTimerText.setCharacterSize(24);
    frameTimerText.setFillColor(sf::Color::Yellow);

    size_t frameCount = 0; //We need to keep track of frame numbers to select the correct thread in the queue to render.
    size_t savedFrameCount = 0;
    auto frameTimer = std::chrono::high_resolution_clock::now();
    while (window.isOpen())
    {
        auto startTime = std::chrono::high_resolution_clock::now(); //lock fps to help make the animation smoother.

        //SFML stuff
        sf::Event event{sf::Event::Closed};
        while(window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }
        window.clear(); //clear the window for the next draw. Disable this for a trippy experience!

        auto &threadInfo = workerThreads[frameCount++ % ThreadCount];
        while(threadInfo.invalidated) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        threadInfo.threadSync.lock();   //Get exclusive access to the vertex data, if the thread is not finished this will block here.
        window.draw(threadInfo.output.getVertices());  //Draw the rendered vertex data
        threadInfo.invalidated = true;  //Tell the thread to draw the next frame.
        threadInfo.threadSync.unlock(); //Give back access to the vertex data

        //fps seems to be too high to measure per-frame so I resorted to counting frames for fractions of a second like a neanderthal.
        constexpr size_t fpsScaleFactor = 1;
        if(std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - frameTimer).count() >=
           1000.0 / fpsScaleFactor)
        {
            frameTimerText.setString(std::to_string((frameCount - savedFrameCount) * fpsScaleFactor)); //Benchmarking
            frameTimer = std::chrono::high_resolution_clock::now();
            savedFrameCount = frameCount;
        }
        window.draw(frameTimerText);
        window.display(); //"display" the rendered data. I actually have no idea what this is supposed to do (swap famebuffers?)

        std::chrono::duration<size_t, std::micro> sleepTimer = std::chrono::duration_cast<std::chrono::microseconds>((std::chrono::high_resolution_clock::now() - startTime) + std::chrono::milliseconds(1));

        //std::this_thread::sleep_for(sleepTimer); - Has a rather large minimum sleep time so no use here.
        sf::sleep(sf::microseconds(sleepTimer.count()));
    }
    for(auto &threadInfo : workerThreads)
    {
        threadInfo.threadSync.lock();
        threadInfo.isRunning = false;
        threadInfo.threadSync.unlock();

        threadInfo.result.get();
    }
    return 0;
}
