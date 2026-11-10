#include "TestFramework.h"
#include "coop/PoseFrame.h"
#include "coop/Protocol.h"
#include <array>
#include <limits>
using namespace coop;

namespace {
std::array<float,16> Matrix(Quat q) {
    return {1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.w*q.z),2*(q.x*q.z-q.w*q.y),0,
            2*(q.x*q.y-q.w*q.z),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z+q.w*q.x),0,
            2*(q.x*q.z+q.w*q.y),2*(q.y*q.z-q.w*q.x),1-2*(q.x*q.x+q.y*q.y),0,
            30,40,50,1};
}
}
TEST(PoseFrame_RowMatrixAllBranchesScaleAndSign) {
    for (Quat q : {Quat{}, Quat{1,0,0,0}, Quat{0,1,0,0}, Quat{0,0,1,0}, Normalize({.2f,.3f,-.7f,.1f})}) {
        auto matrix=Matrix(q);
        for (int row=0;row<3;++row)
            for (int col=0;col<3;++col) matrix[row*4+col]*=float(row+2);
        Quat result;
        REQUIRE(PoseWorldRotation(matrix.data(),result));
        CHECK(AngleBetween(q,result)<.001f);
        CHECK(AngleBetween({-q.x,-q.y,-q.z,-q.w},result)<.001f);
    }
}
TEST(PoseFrame_RejectsNonAffineNonfiniteShearAndReflection) {
    for (int kind=0;kind<5;++kind) {
        auto matrix=Matrix({});
        if (kind==0) matrix[0]=std::numeric_limits<float>::quiet_NaN();
        if (kind==1) matrix[3]=1;
        if (kind==2) matrix[0]=0;
        if (kind==3) matrix[1]=.5f;
        if (kind==4) matrix[0]=-1;
        Quat result;
        CHECK(!PoseWorldRotation(matrix.data(),result));
    }
}
TEST(PoseFrame_ConversionPreservesWorldFacingAndRootOffset) {
    const auto source=Normalize({.1f,.3f,.2f,.7f});
    const auto target=Normalize({-.2f,.1f,.6f,.2f});
    const auto root=Normalize({.4f,.2f,-.3f,.8f});
    const auto delta=Multiply(InverseRotation(target),source);
    CHECK(AngleBetween(Multiply(target,Multiply(delta,root)),Multiply(source,root))<.001f);
    const Vec3 offset{.1f,.2f,.3f};
    CHECK(Distance(Rotate(target,Rotate(delta,offset)),Rotate(source,offset))<.0001f);
}
TEST(PoseFrame_CodecLegacyExtendedAndTruncated) {
    MsgPose source;
    source.pose.rotations={{},{0,0,1,0}};
    source.pose.rootPos={.1f,.2f,.3f};
    for (bool extended : {false,true}) {
        source.pose.hasWorldRotation=extended;
        source.pose.worldRotation=Normalize({.2f,.3f,.4f,.5f});
        ByteWriter writer;
        source.Write(writer);
        CHECK(writer.Data().size()==42+(extended?4:0));
        ByteReader reader(writer.Data());
        MsgPose dest;
        REQUIRE(dest.Read(reader));
        CHECK(reader.AtEnd());
        CHECK(dest.pose.hasWorldRotation==extended);
        CHECK(dest.pose.rotations.size()==2);
        if (extended) CHECK(AngleBetween(dest.pose.worldRotation,source.pose.worldRotation)<.005f);
        for (size_t length=0;length<writer.Data().size();++length) {
            ByteReader shortReader(writer.Data().data(),length);
            MsgPose truncated;
            CHECK(!truncated.Read(shortReader));
        }
    }
}
TEST(PoseFrame_InterpolationWrapAndMissingMetadata) {
    Pose a,b;
    a.rotations={{}};b.rotations={{}};
    a.hasWorldRotation=b.hasWorldRotation=true;
    a.worldRotation={0,0,.9999619f,.0087265f};
    b.worldRotation={0,0,-.9999619f,.0087265f};
    auto mid=Lerp(a,b,.5f);
    CHECK(mid.hasWorldRotation);
    CHECK(AngleBetween(mid.worldRotation,{0,0,1,0})<.001f);
    b.hasWorldRotation=false;
    CHECK(!Lerp(a,b,.5f).hasWorldRotation);
}
