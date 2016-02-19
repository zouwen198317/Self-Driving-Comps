#include "sdcVisibleObject.hh"

using namespace gazebo;

// How far off our estimates can be before we assume we're seeing a differnt object
const double sdcVisibleObject::UNCERTAINTY_RATIO = 0.3;

/*
 * Visible objects are obstructions detected by Lidar rays. They have estimated
 * movement parameters in order to help us predict their motion and track them
 * across multiple sensor readings.
 */
sdcVisibleObject::sdcVisibleObject(sdcLidarRay right, sdcLidarRay left, double dist){
    this->left = left;
    this->right = right;
    this->dist = dist;

    this->centerpoint = this->GetCenterPoint();
    this->prevPoints.push_back(this->centerpoint);

    this->estimatedXSpeed = 0;
    this->estimatedYSpeed = 0;
    this->estimatedDirection = (left.angle + right.angle) / 2.;
    this->confidence = 0.01;

    this->tracking = false;

    // std::cout << left.GetLateralDist() << "\t" << left.GetLongitudinalDist() << "\t" << right.GetLateralDist() << "\t" << right.GetLongitudinalDist() << std::endl;
}

/*
 * Returns true if the given object is a possible new position of this object
 */
bool sdcVisibleObject::IsSameObject(sdcVisibleObject other){
    math::Vector2d estPos = this->EstimateUpdate();
    double uncertainty = sqrt(pow(estPos.x - other.centerpoint.x, 2) + pow(estPos.y - other.centerpoint.y, 2));

    // std::cout << "CUR \t" << this->centerpoint.x << "\t" << this->centerpoint.y << std::endl;
    // std::cout << "EST \t" << estPos.x << "\t" << estPos.y << std::endl;
    // std::cout << "OTH \t" << other.centerpoint.x << "\t" << other.centerpoint.y << std::endl;
    // std::cout << "UNC \t" << uncertainty << "\t" << confidence << std::endl;
    // std::cout << "LINE \t" << this->lineSlope << "\t" << this->lineIntercept << std::endl;
    // std::cout << (uncertainty * confidence < UNCERTAINTY_RATIO) << std::endl << std::endl;

    return uncertainty * confidence < UNCERTAINTY_RATIO;
}

/*
 * Calculates an estimated new position this object would be at with it's given estimated
 * speed and direction
 */
math::Vector2d sdcVisibleObject::EstimateUpdate(){
    // double newX = this->centerpoint.x + sin(this->estimatedDirection.angle) * this->estimatedSpeed;
    // double newY = this->centerpoint.y + cos(this->estimatedDirection.angle) * this->estimatedSpeed;
    // return math::Vector2d(newX, newY);
    double newX = this->centerpoint.x + this->estimatedXSpeed;
    double newY = this->centerpoint.y + this->estimatedYSpeed;
    return math::Vector2d(newX, newY);
}

math::Vector2d sdcVisibleObject::GetProjectedPosition(int numSteps){
    double newX = this->centerpoint.x + this->estimatedXSpeed * numSteps;
    double newY = this->centerpoint.y + this->estimatedYSpeed * numSteps;
    return math::Vector2d(newX, newY);
}

/*
 * Given new readings for the location of this object, update the stored parameters
 * to learn it's projected speed and direction
 */
void sdcVisibleObject::Update(sdcLidarRay newLeft, sdcLidarRay newRight, double newDist){
    this->confidence = fmin(1.0, this->confidence + 0.01);

    math::Vector2d newCenterpoint = this->GetCenterPoint(newLeft, newRight, newDist);

    double newEstimatedXSpeed = newCenterpoint.x - this->centerpoint.x;
    double newEstimatedYSpeed = newCenterpoint.y - this->centerpoint.y;

    double alpha = fmax((.1 + newDist * .005), (.2 - newDist * .005));
    newEstimatedXSpeed = (alpha * newEstimatedXSpeed) + ((1 - alpha) * this->estimatedXSpeed);
    newEstimatedYSpeed = (alpha * newEstimatedYSpeed) + ((1 - alpha) * this->estimatedYSpeed);

    this->estimatedXSpeed = newEstimatedXSpeed;
    this->estimatedYSpeed = newEstimatedYSpeed;
    // this->estimatedDirection = sdcAngle(atan2(newCenterpoint.x - this->centerpoint.x, newCenterpoint.y - this->centerpoint.y));


    alpha = .036197; // Determined through repeated experimentation (a.k.a. Divine Inspiration (a.a.k.a. Jon made it up))
    math::Vector2d newLineCoefficients = this->FitLineToPoints(this->prevPoints, newCenterpoint);
    this->lineSlope = newLineCoefficients.x;
    this->lineIntercept = newLineCoefficients.y;

    if(this->prevPoints.size() > 15){
        this->prevPoints.erase(this->prevPoints.begin());
    }
    this->prevPoints.push_back(newCenterpoint);

    this->centerpoint = newCenterpoint;

    this->left = newLeft;
    this->right = newRight;
    this->dist = newDist;
}

math::Vector2d sdcVisibleObject::FitLineToPoints(std::vector<math::Vector2d> points, math::Vector2d newPoint){
    int numPoints = points.size();

    double sumX=0, sumY=0, sumXY=0, sumX2=0;
    for(int i=0; i<numPoints; i++) {
      sumX += points[i].x;
      sumY += points[i].y;
      sumXY += points[i].x * points[i].y;
      sumX2 += points[i].x * points[i].x;
    }

    sumX += newPoint.x;
    sumY += newPoint.y;
    sumXY += newPoint.x * newPoint.y;
    sumX2 += newPoint.x * newPoint.x;

    double xMean = sumX / (numPoints + 1);
    double yMean = sumY / (numPoints + 1);

    double denom = sumX2 - sumX * xMean;

    double slope = (sumXY - sumX * yMean) / denom;
    double yInt = yMean - slope * xMean;

    return math::Vector2d(slope, yInt);
}

void sdcVisibleObject::Update(sdcVisibleObject newObject){
    this->Update(newObject.left, newObject.right, newObject.dist);
}

/*
 * Set whether we are tracking this object
 */
void sdcVisibleObject::SetTracking(bool isTracking){
    this->tracking = isTracking;
}

/*
 * Gets the centerpoint of this object based on the left and right rays
 */
math::Vector2d sdcVisibleObject::GetCenterPoint(){
    return this->GetCenterPoint(this->left, this->right, this->dist);
}

/*
 * Gets the centerpoint of the two given rays in (x,y) coordinates
 */
math::Vector2d sdcVisibleObject::GetCenterPoint(sdcLidarRay left, sdcLidarRay right, double dist){
    sdcAngle midAngle = (left.angle + right.angle) / 2.;
    sdcLidarRay midRay = sdcLidarRay(midAngle, dist);
    double x = midRay.GetLateralDist();
    double y = midRay.GetLongitudinalDist();

    // double x = (left.GetLateralDist() + right.GetLateralDist()) / 2.;
    // double y = (left.GetLongitudinalDist() + right.GetLongitudinalDist()) / 2.;
    return math::Vector2d(x, y);
}
