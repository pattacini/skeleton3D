/* Copyright (C) 2017 RobotCub Consortium
 * Author:  Phuong Nguyen
 * email:   phuong.nguyen@iit.it
 * website: www.robotcub.org
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/"+robot+"/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/
#ifndef VTCALIB_H
#define VTCALIB_H

#include <stdio.h>
#include <string>
#include <cctype>
#include <algorithm>
#include <map>

#include <yarp/os/all.h>
#include <yarp/dev/all.h>
#include <yarp/sig/all.h>
#include <yarp/math/Math.h>

#include <iCub/ctrl/math.h>
#include <iCub/skinDynLib/common.h>

#include <gsl/gsl_math.h>

#include <iCub/iKin/iKinFwd.h>

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include <set>
#include <list>

#include <iCub/ctrl/math.h>
#include <iCub/periPersonalSpace/skinPartPWE.h>
#include <iCub/skinDynLib/skinContact.h>
#include <iCub/skinDynLib/skinContactList.h>

#define SKIN_THRES	        7 // Threshold with which a contact is detected

using namespace std;
using namespace yarp;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;
using namespace yarp::dev;
using namespace iCub::iKin;
using namespace iCub::ctrl;
using namespace iCub::skinDynLib;

class vtCalib: public yarp::os::RFModule
{
protected:
    string          name;
    string          robot;
    double          arm_version;
    double          period;
    int             verbosity;
    double          timeNow;

    vector<Vector>  contactPts;     //!< vector of skin contact points at a moment


    // Driver for "classical" interfaces
    PolyDriver          ddR;        //!< right arm device driver
    PolyDriver          ddL;        //!< left arm  device driver
    PolyDriver          ddT;        //!< torso controller  driver
    PolyDriver          ddH;        //!< head  controller  driver
    PolyDriver          ddG;        //!< gaze  controller  driver

    // "Classical" interfaces - RIGHT ARM
    IEncoders           *iencsR;
    yarp::sig::Vector   *encsR;
    iCubArm             *armR;
    int                 jntsR;  //all joints including fingers ~ 16
    int                 jntsAR; //arm joints only ~ 7
    // "Classical" interfaces - LEFT ARM
    IEncoders           *iencsL;
    yarp::sig::Vector   *encsL;
    iCubArm             *armL;
    int                 jntsL;  //all joints including fingers ~ 16
    int                 jntsAL; //arm joints only ~ 7
    // "Classical" interfaces - TORSO
    IEncoders           *iencsT;
    yarp::sig::Vector   *encsT;
    int                 jntsT;
    // "Classical" interfaces - HEAD
    IEncoders           *iencsH;
    yarp::sig::Vector   *encsH;
    int                 jntsH;
    // Gaze controller interface
    IGazeControl        *igaze;
    int contextGaze;

    ResourceFinder    armsRF;

    //N.B. All angles in this thread are in degrees
    yarp::sig::Vector qL;           //!< current values of left arm joints (should be 7)
    yarp::sig::Vector qR;           //!< current values of right arm joints (should be 7)
    yarp::sig::Vector qT;           //!< current values of torso joints (3, in the order expected for iKin: yaw, roll, pitch)

    // Stamp for the setEnvelope for the ports
    yarp::os::Stamp ts;

    vector<string>          filenames;
    vector <skinPartPWE>    iCubSkin;
    int                     iCubSkinSize;

    string                  modality;                                       //!< modality to use (either 1D or 2D)

    BufferedPort<iCub::skinDynLib::skinContactList> skinPortIn;             //!< input from the skinManager
    Port                                            contactDumperPortOut;   //!< output to dump the contact points in World FoR

    //********************************************
    // From vtRFThread
    bool detectContact(iCub::skinDynLib::skinContactList *_sCL, int &idx,
                                   std::vector <unsigned int> &idv);

    //see also Compensator::setTaxelPosesFromFile in icub-main/src/modules/skinManager/src/compensator.cpp
    // From vtRFThread
    /**
    * Finds out the positions of the taxels w.r.t. their respective limbs
    **/
    bool setTaxelPosesFromFile(const string filePath, skinPartPWE &sP);

    // From vtRFThread
    /**
    * If it is defined for the respective skin part, it fills the taxelIDtoRepresentativeTaxelID vector that is indexed by taxel IDs
    * and returns taxel IDs of their representatives - e.g. triangle centers.
    **/
    void initRepresentativeTaxels(skinPart &sP);

    // From vtRFThread
    /**
    * For all active taxels, it returns a set of "representative" active taxels if they were defined for the respective skin part
    * E.g. if at least one taxel from a triangle was active, the center of this triangle will be part of the output list
    * @param IDv  is a vector of IDs of the taxels activated
    * @param IDx  is the index of the iCubSkin affected by the contact
                  (basically, the index of the skinPart that has been touched)
    * @param v    is a vector of IDs of the representative taxels activated
    **/
    bool getRepresentativeTaxels(const std::vector<unsigned int> IDv, const int IDx,
                                 std::vector<unsigned int> &v);

    // From vtRFThread
    bool readEncodersAndUpdateArmChains();

    // From vtRFThread
    yarp::sig::Vector locateTaxel(const yarp::sig::Vector &_pos, const string &part);

    // From vtRFThread
    int printMessage(const int l, const char *f, ...) const;

    void vector2bottle(const std::vector<Vector> &vec, yarp::os::Bottle &b);

public:
    bool    configure(ResourceFinder &rf);
    bool    respond(const Bottle &command, Bottle &reply);
    bool    updateModule();
    double  getPeriod();
    bool    close();
};

#endif // VTCALIB_H
