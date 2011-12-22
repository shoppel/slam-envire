#include "Core.hpp"
#include "maps/Pointcloud.hpp"
#include "tools/PlyFile.hpp"

#include <fstream>

using namespace envire;

ENVIRONMENT_ITEM_DEF( Pointcloud )

const std::string Pointcloud::VERTEX_COLOR = "vertex_color";
const std::string Pointcloud::VERTEX_NORMAL = "vertex_normal";
const std::string Pointcloud::VERTEX_VARIANCE = "vertex_variance";
const std::string Pointcloud::VERTEX_ATTRIBUTES = "vertex_attributes";

Pointcloud::Pointcloud()
{
}

Pointcloud::~Pointcloud()
{
}

Pointcloud::Pointcloud(Serialization& so, bool handleMap)
{
    unserialize(so, handleMap);
}

void Pointcloud::serialize(Serialization& so)
{
    serialize(so, true);
}

void Pointcloud::serialize(Serialization& so, bool handleMap)
{
    CartesianMap::serialize(so);

    if(handleMap)
	writePly( getMapFileName(so.getMapPath()) + ".ply" );
}

void Pointcloud::unserialize(Serialization& so, bool handleMap)
{
    CartesianMap::unserialize(so);

    if(handleMap)
    {
    if( !readPly( getMapFileName(so.getMapPath()) + ".ply" ) )
        readText( getMapFileName(so.getMapPath()) + ".txt" );
    }
}

bool Pointcloud::writePly(const std::string& path)
{
    PlyFile ply(path);
    return ply.serialize( this );
}

bool Pointcloud::readPly(const std::string& path)
{
    PlyFile ply(path);
    return ply.unserialize( this );
}

bool Pointcloud::writeText(const std::string& path)
{
    std::ofstream data(path.c_str());
    if( data.fail() )  
    {
	std::cerr << "Could not open file '" + path + "' for writing." << std::endl;
	return false;
    }

    for(size_t i=0;i<vertices.size();i++)
    {
	data << vertices[i].x() << " " << vertices[i].y() << " " << vertices[i].z() << std::endl;
    }
    
    return true;
}

bool Pointcloud::readText(const std::string& path)
{
    std::ifstream data(path.c_str());
    if( data.fail() )  
    {
	std::cerr << "Could not open file '" + path + "'." << std::endl;
	return false;
    }

    while( !data.eof() )
    {
	double x, y, z;
	data >> x >> y >> z;
	vertices.push_back( Eigen::Vector3d( x,y,z ) );
    }

    return true;
}

Pointcloud* Pointcloud::importCsv(const std::string& path, FrameNode* fm)
{
    Pointcloud* pc = new Pointcloud();
    pc->readText( path );

    Environment* env = fm->getEnvironment();
    env->attachItem(pc);
    pc->setFrameNode(fm);

    return pc;
}

void Pointcloud::copyFrom( Pointcloud* source, bool transform )
{
    // TODO only copies the vertices for now
    clear();

    // get relative transform 
    Transform t = source->getFrameNode()->relativeTransform( getFrameNode() );
    bool needsTransform = !t.isApprox( Transform( Transform::Identity() ) );

    if( !transform || !needsTransform )
    {
	vertices = source->vertices;
    }
    else
    {
	for( std::vector<Eigen::Vector3d>::iterator it = source->vertices.begin(); it != source->vertices.end(); it ++ )
	    vertices.push_back( t * *it );
    }
}

Pointcloud::Extents Pointcloud::getExtents() const
{
    //TODO: Implement some sort of caching
    Extents res;
    for(size_t i=0;i<vertices.size();i++)
    {
	res.extend( vertices[i] );
    }
    return res; 
}
