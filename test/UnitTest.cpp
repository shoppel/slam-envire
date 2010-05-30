#include <Eigen/Core>
#include <Eigen/LU>

#include "envire/Core.hpp"
#include "envire/LaserScan.hpp"
#include "envire/TriMesh.hpp"
#include "envire/ScanMeshing.hpp"

#define BOOST_TEST_MODULE EnvireTest 
#include <boost/test/included/unit_test.hpp>
#include <boost/scoped_ptr.hpp>

#include "envire/GridAccess.hpp"
#include "envire/Grids.hpp"
   
using namespace envire;
using namespace std;

template<class T> bool contains(const std::list<T>& list, const T& element)
{
    return find( list.begin(), list.end(), element ) != list.end();
};

class DummyOperator : public Operator 
{
public:
    bool updateAll() {};
    void serialize(Serialization &) {};
};

class DummyLayer : public Layer 
{
public:
    Layer* clone() {return new DummyLayer(*this);};
    void serialize(Serialization &) {};
};

class DummyCartesianMap : public CartesianMap 
{
public:
    CartesianMap* clone() {return new DummyCartesianMap(*this);};
    void serialize(Serialization &) {};
};

BOOST_AUTO_TEST_CASE( TreeTest )
{
    // set up an environment
    boost::scoped_ptr<Environment> env( new Environment() );

    //check if three nodes are the childs of root
    // create some child framenodes
    FrameNode *fn1, *fn2, *fn3;
    fn1 = new FrameNode();
    fn1->setTransform( 
	    Eigen::Transform3d(Eigen::Translation3d( 0.0, 0.0, 0.5 )) );
    fn2 = new FrameNode();
    fn2->setTransform( 
	    Eigen::Transform3d(Eigen::Quaterniond(0.0, 1.0, 0.0, 0.0 )));
    fn3 = new FrameNode();

    //attach 3 node to root
    env->addChild(env->getRootNode(), fn1);
    env->addChild(env->getRootNode(), fn2);
    env->addChild(env->getRootNode(), fn3);
    
    std::list<FrameNode *> childs = env->getChildren(env->getRootNode());
    
    BOOST_CHECK( contains(env->getChildren(env->getRootNode()),fn1) );
    BOOST_CHECK( contains(env->getChildren(env->getRootNode()),fn2) );
    BOOST_CHECK( contains(env->getChildren(env->getRootNode()),fn3) );
    
    //attach fn2 to fn3
    //this will of course detach fn2 from the rootnode and attach it to fn3
    env->addChild(fn3, fn2);
    
    BOOST_CHECK( contains(env->getChildren(fn3) ,fn2) );
    
    //remove fn2 from rootNode
    env->removeChild(env->getRootNode(), fn2);
    BOOST_CHECK( contains(env->getChildren(fn3) ,fn2) );
    BOOST_CHECK(!contains(env->getChildren(env->getRootNode()) ,fn2) );
}

BOOST_AUTO_TEST_CASE( environment )
{
    // set up an environment
    boost::scoped_ptr<Environment> env( new Environment() );

    // an environment should always have a root node 
    BOOST_CHECK( env->getRootNode() );

    // create some child framenodes
    FrameNode *fn1, *fn2, *fn3;
    fn1 = new FrameNode();
    fn1->setTransform( 
	    Eigen::Transform3d(Eigen::Translation3d( 0.0, 0.0, 0.5 )) );
    fn2 = new FrameNode();
    fn2->setTransform( 
	    Eigen::Transform3d(Eigen::Quaterniond(0.0, 1.0, 0.0, 0.0 )));
    fn3 = new FrameNode();
    fn3->setTransform( 
	    Eigen::Transform3d(Eigen::Translation3d(0.0, 1.0, 0.0)));
    
    // attach explicitely
    env->attachItem( fn1 );
    BOOST_CHECK( fn1->isAttached() );
    BOOST_CHECK_EQUAL( fn1->getEnvironment(), env.get() );
    env->addChild( env->getRootNode(), fn1 );
    BOOST_CHECK_EQUAL( env->getRootNode(), env->getParent(fn1) );
    BOOST_CHECK( contains(env->getChildren(env->getRootNode()),fn1) );

    // implicit attachment
    env->addChild( fn1, fn2 );
    BOOST_CHECK( fn2->isAttached() );
    
    // setup the rest of the framenodes
    env->addChild( env->getRootNode(), fn3 );    

    // perform a relative transformation
    FrameNode::TransformType rt1 = env->relativeTransform(fn2, fn1);
    BOOST_CHECK( rt1.matrix().isApprox( fn2->getTransform().matrix(), 1e-10 ) );

    FrameNode::TransformType rt2 = env->relativeTransform(fn2, fn3);
    BOOST_CHECK( rt2.matrix().isApprox( 
		fn3->getTransform().inverse() * fn1->getTransform() * fn2->getTransform(),
		1e-10 ) );
    
    // now do the same for layers
    Layer *l1, *l2, *l3;
    l1 = new DummyLayer();
    l2 = new DummyLayer();
    l3 = new DummyLayer();

    env->attachItem( l1 );
    BOOST_CHECK( l1->isAttached() );
    BOOST_CHECK_EQUAL( l1->getEnvironment(), env.get() );

    env->addChild( l1, l2 );
    BOOST_CHECK_EQUAL( l1, env->getParent(l2) );
    BOOST_CHECK( contains(env->getChildren(l1),l2) );

    env->attachItem(l3);

    // CartesianMaps should work the same
    CartesianMap *m1, *m2;
    m1 = new DummyCartesianMap();
    m2 = new DummyCartesianMap();

    env->attachItem( m1 );
    env->attachItem( m2 );

    env->setFrameNode( m1, fn1 );
    env->setFrameNode( m2, fn1 );

    BOOST_CHECK_EQUAL( env->getFrameNode( m1 ), fn1 );
    BOOST_CHECK( contains(env->getMaps(fn1),m1) );
    BOOST_CHECK( contains(env->getMaps(fn1),m2) );
    
    // now to operators
    Operator *o1;
    o1 = new DummyOperator();
    env->attachItem( o1 );

    env->addInput( o1, l1 );
    env->addInput( o1, l2 );
    env->addOutput( o1, l3 );

    BOOST_CHECK( contains(env->getInputs(o1),l1) );
    BOOST_CHECK( contains(env->getOutputs(o1),l3) );
}

BOOST_AUTO_TEST_CASE( serialization )
{
    Serialization so;
    boost::scoped_ptr<Environment> env( new Environment() );

    // create some child framenodes
    FrameNode *fn1, *fn2, *fn3;
    fn1 = new FrameNode();
    fn1->setTransform( 
	    Eigen::Transform3d(Eigen::Translation3d( 0.0, 0.0, 0.5 )) );
    fn2 = new FrameNode();
    fn2->setTransform( 
	    Eigen::Transform3d(Eigen::Quaterniond(0.0, 1.0, 0.0, 0.0 )));
    fn3 = new FrameNode();
    
    // attach explicitely
    env->attachItem( fn1 );
    env->addChild( env->getRootNode(), fn1 );
    env->addChild( fn1, fn2 );
    env->addChild( env->getRootNode(), fn3 );

    // TODO get cmake to somehow add an absolute path here
    std::string path("build/test");
    so.serialize(env.get(), path);

    // now try to parse the thing again
    boost::scoped_ptr<Environment> env2(so.unserialize( "build/test" ));

    // TODO check that the structure is the same
}

BOOST_AUTO_TEST_CASE( functional ) 
{
    boost::scoped_ptr<Environment> env( new Environment() );

    LaserScan* scan = LaserScan::importScanFile("test/test.scan", env->getRootNode() );

    // create a TriMesh Layer and attach it to the root Node.
    TriMesh* mesh = new TriMesh();
    env->attachItem( mesh );

    // set up a meshing operator on the output mesh. Add then an input
    // and parametrize the meshing operation. 
    ScanMeshing* mop = new ScanMeshing();
    env->attachItem( mop );

    mop->setMaxEdgeLength(0.5);

    mop->addInput(scan);
    mop->addOutput(mesh);

    mop->updateAll();

    Serialization so;
    so.serialize(env.get(), "build/test");
}

BOOST_AUTO_TEST_CASE( grid_access ) 
{
    boost::scoped_ptr<Environment> env( new Environment() );
    ElevationGrid *m1 = new ElevationGrid( 2, 2, 1, 1 );
    ElevationGrid *m2 = new ElevationGrid( 2, 2, 1, 1 );

    env->attachItem( m1 );
    env->attachItem( m2 );

    FrameNode *fn1 = new FrameNode();
    fn1->setTransform( 
	    Eigen::Transform3d(Eigen::Translation3d( 0.0, 0.0, 0.0 )) );
    FrameNode *fn2 = new FrameNode();
    fn2->setTransform( 
	    Eigen::Transform3d(Eigen::Translation3d( 5.0, 0.0, 0.0 )) );

    env->addChild( env->getRootNode(), fn1 );
    env->addChild( env->getRootNode(), fn2 );

    m1->setFrameNode( fn1 );
    m2->setFrameNode( fn2 );

    for(int i=0;i<2;i++)
    {
	for(int j=0;j<2;j++)
	{
	    m1->getGridData()[i][j] = i*2 + j;
	    m2->getGridData()[i][j] = i*2 + j;
	}
    }

    Eigen::Vector3d p1(0.5,0.5,0);
    Eigen::Vector3d p2(4.5,0.5,0);
    Eigen::Vector3d p3(6.5,0.5,0);

    GridAccess ga( env.get() );
    bool r1 = ga.getElevation( p1 );
    bool r2 = ga.getElevation( p2 );
    bool r3 = ga.getElevation( p3 );

    cout << r1 << ":" << p1.transpose() << endl;
    cout << r2 << ":" << p2.transpose() << endl;
    cout << r3 << ":" << p3.transpose() << endl;
}

// EOF
//
