#include "SimpleTraversability.hpp"
#include <base/logging.h>

using namespace envire;
using envire::Grid;

ENVIRONMENT_ITEM_DEF( SimpleTraversability );
/* For backward compatibility reasons */
static envire::SerializationPlugin< SimpleTraversability >  nav_graph_search_TraversabilityClassifier("nav_graph_search::TraversabilityClassifier");
/* For backward compatibility reasons */
static envire::SerializationPlugin< SimpleTraversability >  envire_MLSSimpleTraversability("envire::MLSSimpleTraversability");

SimpleTraversability::SimpleTraversability()
{
}

SimpleTraversability::SimpleTraversability(
        SimpleTraversabilityConfig const& conf)
    : Operator(0, 1)
    , conf(conf)
{
}

SimpleTraversability::SimpleTraversability(
        double weight_force,
        double force_threshold,
        int class_count,
        double min_width,
        double ground_clearance)
    : Operator(0, 1)
{
    conf.weight_force     = weight_force;
    conf.force_threshold  = force_threshold;
    conf.class_count      = class_count;
    conf.min_width        = min_width;
    conf.ground_clearance = ground_clearance;
}

void SimpleTraversability::serialize(envire::Serialization& so)
{
    Operator::serialize(so);

    for (int i = 0; i < INPUT_COUNT; ++i)
    {
        if (input_layers_id[i] != "" && !input_bands[i].empty())
        {
            so.write("input" + boost::lexical_cast<std::string>(i), input_layers_id[i]);
            so.write("input" + boost::lexical_cast<std::string>(i) + "_band", input_bands[i]);
        }
    }

    so.write("weight_force", conf.weight_force);
    so.write("force_threshold", conf.force_threshold);
    so.write("class_count", conf.class_count);
    so.write("ground_clearance", conf.ground_clearance);
    so.write("min_width", conf.min_width);
    so.write("output_band", output_band);
}

void SimpleTraversability::unserialize(envire::Serialization& so)
{
    Operator::unserialize(so);
    
    for (int i = 0; i < INPUT_COUNT; ++i)
    {
        std::string input_key = "input" + boost::lexical_cast<std::string>(i);
        if (so.hasKey(input_key))
        {
            input_layers_id[i] = so.read<std::string>(input_key);
            input_bands[i] = so.read<std::string>(input_key + "_band");
        }
    }

    so.read<double>("weight_force", conf.weight_force);
    so.read<double>("force_threshold", conf.force_threshold);
    so.read<int>("class_count", conf.class_count);
    so.read<double>("ground_clearance", conf.ground_clearance);
    so.read<double>("min_width", conf.min_width);
    so.read<std::string>("output_band", output_band);
}

envire::Grid<float>* SimpleTraversability::getInputLayer(INPUT_DATA index) const
{
    if (input_layers_id[index] == "")
        return 0;
    return getEnvironment()->getItem< Grid<float> >(input_layers_id[index]).get();
}
std::string SimpleTraversability::getInputBand(INPUT_DATA index) const
{ return input_bands[index]; }

envire::Grid<float>* SimpleTraversability::getSlopeLayer() const { return getInputLayer(SLOPE); }
std::string SimpleTraversability::getSlopeBand() const { return getInputBand(SLOPE); }
void SimpleTraversability::setSlope(Grid<float>* grid, std::string const& band_name)
{
    addInput(grid);
    input_layers_id[SLOPE] = grid->getUniqueId();
    input_bands[SLOPE] = band_name;

}

envire::Grid<float>* SimpleTraversability::getMaxStepLayer() const { return getInputLayer(MAX_STEP); }
std::string SimpleTraversability::getMaxStepBand() const { return getInputBand(MAX_STEP); }
void SimpleTraversability::setMaxStep(Grid<float>* grid, std::string const& band_name)
{
    addInput(grid);
    input_layers_id[MAX_STEP] = grid->getUniqueId();
    input_bands[MAX_STEP] = band_name;
}

envire::Grid<float>* SimpleTraversability::getMaxForceLayer() const { return getInputLayer(MAX_FORCE); }
std::string SimpleTraversability::getMaxForceBand() const { return getInputBand(MAX_FORCE); }
void SimpleTraversability::setMaxForce(Grid<float>* grid, std::string const& band_name)
{
    addInput(grid);
    input_layers_id[MAX_FORCE] = grid->getUniqueId();
    input_bands[MAX_FORCE] = band_name;
}

void SimpleTraversability::setOutput(OutputLayer* grid, std::string const& band_name)
{
    removeOutputs();
    addOutput(grid);
    output_band = band_name;
}

bool SimpleTraversability::updateAll()
{
    OutputLayer* output_layer = getOutput< OutputLayer* >();
    if (!output_layer)
        throw std::runtime_error("SimpleTraversability: no output band set");

    OutputLayer::ArrayType& result = output_band.empty() ?
        output_layer->getGridData() :
        output_layer->getGridData(output_band);

    if (output_band.empty())
        output_layer->setNoData(CLASS_UNKNOWN);
    else
        output_layer->setNoData(output_band, CLASS_UNKNOWN);

    static float const DEFAULT_UNKNOWN_INPUT = -std::numeric_limits<float>::infinity();
    Grid<float> const* input_layers[3] = { 0, 0, 0 };
    float input_unknown[3];

    boost::multi_array<float, 2> const* inputs[3] = { 0, 0, 0 };
    bool has_data = false;
    for (int i = 0; i < INPUT_COUNT; ++i)
    {
        if (input_layers_id[i] != "" && !input_bands[i].empty())
        {
            input_layers[i] = getEnvironment()->getItem< Grid<float> >(input_layers_id[i]).get();
            has_data = true;
            inputs[i] = &(input_layers[i]->getGridData(input_bands[i]));

            std::pair<float, bool> no_data = input_layers[i]->getNoData(input_bands[i]);
            if (no_data.second)
            {
                std::cout << "band " << i << " no_data=" << no_data.first << std::endl;
                input_unknown[i] = no_data.first;
            }
            else
                input_unknown[i] = DEFAULT_UNKNOWN_INPUT;
        }
    }
    if (!has_data)
        throw std::runtime_error("SimpleTraversability: no input layer configured");

    if (inputs[MAX_STEP] && conf.ground_clearance == 0)
        throw std::runtime_error("a max_step band is available, but the ground clearance is set to zero");

    double const class_width = 1.0 / conf.class_count;
                
    int width = output_layer->getWidth(), height = output_layer->getHeight();
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // Read the values for this cell. Set to CLASS_UNKNOWN and ignore the cell
            // if one of the available input bands has no information
            double values[INPUT_COUNT];
            bool has_value[INPUT_COUNT];
            for (int band_idx = 0; band_idx < INPUT_COUNT; ++band_idx)
            {
                has_value[band_idx] = false;
                if (!inputs[band_idx]) continue;

                double value = (*inputs[band_idx])[y][x];
                if (value != input_unknown[band_idx])
                {
                    values[band_idx] = value;
                    has_value[band_idx] = true;
                }
            }

            bool has_slope = has_value[SLOPE],
                 has_max_step    = has_value[MAX_STEP],
                 has_max_force   = has_value[MAX_FORCE];

            // First, max_step is an ON/OFF threshold on the ground clearance
            // parameter
            if (has_max_step && (values[MAX_STEP] > conf.ground_clearance))
            {
                result[y][x] = CLASS_OBSTACLE;
                continue;
            }
            if ((inputs[MAX_FORCE] && !has_value[MAX_FORCE]) || (inputs[SLOPE] && !has_value[SLOPE]))
            {
                result[y][x] = CLASS_UNKNOWN;
                continue;
            }

            // Compute an estimate of the force that the system can use to
            // propulse itself, using the max_force / slope values (and using
            // the maximum values if none are available)
            //
            // The result is then a linear mapping of F=[O, force_threshold] to
            // [0, 1]
            double max_force = conf.force_threshold;
            double speed = 1;
            if (has_max_force)
                max_force = values[MAX_FORCE];
            if (has_slope)
                max_force = max_force - conf.weight_force * fabs(sin(values[SLOPE]));

            if (max_force <= 0)
                result[y][x] = CLASS_OBSTACLE;
            else
            {
                if (max_force < conf.force_threshold)
                    speed *= max_force / conf.force_threshold;

                int klass = rint(speed / class_width);
                result[y][x] = CUSTOM_CLASSES + klass;
            }
        }
    }

    closeNarrowPassages(*output_layer, output_band, conf.min_width);
    return true;
}

struct RadialLUT
{
    int centerx, centery;
    unsigned int width, height;
    boost::multi_array<std::pair<int, int>, 2>  parents;
    boost::multi_array<bool, 2> in_distance;

    void precompute(double distance, double scalex, double scaley)
    {
        double const radius2 = distance * distance;

        width  = 2* ceil(distance / scalex) + 1;
        height = 2* ceil(distance / scaley) + 1;
        in_distance.resize(boost::extents[height][width]);
        std::fill(in_distance.data(), in_distance.data() + in_distance.num_elements(), false);
        parents.resize(boost::extents[height][width]);
        std::fill(parents.data(), parents.data() + parents.num_elements(), std::make_pair(-1, -1));

        centerx = width  / 2;
        centery = height / 2;
        parents[centery][centerx] = std::make_pair(-1, -1);

        for (unsigned int y = 0; y < height; ++y)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                int dx = (centerx - x);
                int dy = (centery - y);
                if (dx == 0 && dy == 0) continue;

                double d2 = dx * dx * scalex * scalex + dy * dy * scaley * scaley;
                in_distance[y][x] = (d2 < radius2);
                if (abs(dx) > abs(dy))
                {
                    int parentx = x + dx / abs(dx);
                    int parenty = y + rint(static_cast<double>(dy) / abs(dx));
                    parents[y][x] = std::make_pair(parentx, parenty);
                }
                else
                {
                    int parentx = x + rint(static_cast<double>(dx) / abs(dy));
                    int parenty = y + dy / abs(dy);
                    parents[y][x] = std::make_pair(parentx, parenty);
                }
            }
        }
    }

    void markAllRadius(boost::multi_array<uint8_t, 2>& result, int result_width, int result_height, int centerx, int centery, int value)
    {
        int base_x = centerx - this->centerx;
        int base_y = centery - this->centery;
        for (unsigned int y = 0; y < height; ++y)
        {
            int map_y = base_y + y;
            if (map_y < 0 || map_y >= result_height)
                continue;

            for (unsigned int x = 0; x < width; ++x)
            {
                int map_x = base_x + x;
                if (map_x < 0 || map_x >= result_width)
                    continue;
                if (in_distance[y][x] && result[map_y][map_x] == value)
                {
                    LOG_DEBUG("  found cell with value %i (expected %i) at %i %i, marking radius", result[map_y][map_x], value, map_x, map_y);
                    markSingleRadius(result, centerx, centery, x, y, value, 255);
                }
            }
        }
    }

    void markSingleRadius(boost::multi_array<uint8_t, 2>& result, int centerx, int centery, int x, int y, int expected_value, int mark_value)
    {
        boost::tie(x, y) = parents[y][x];
        while (x != -1 && y != -1)
        {
            int map_x = centerx + x - this->centerx;
            int map_y = centery + y - this->centery;
            uint8_t& current = result[map_y][map_x];
            if (current != expected_value)
	    {
		current = mark_value;
		LOG_DEBUG("  marking %i %i", map_x, map_y);
	    }
            boost::tie(x, y) = parents[y][x];
        }
    }
};

void SimpleTraversability::closeNarrowPassages(SimpleTraversability::OutputLayer& map, std::string const& band_name, double min_width)
{
    RadialLUT lut;
    lut.precompute(min_width, map.getScaleX(), map.getScaleY());
    for (unsigned int y = 0; y < lut.height; ++y)
    {
        for (unsigned int x = 0; x < lut.width; ++x)
            std::cout << "(" << lut.parents[y][x].first << " " << lut.parents[y][x].second << ") ";
        std::cout << std::endl;
    }
    std::cout << std::endl;
    for (unsigned int y = 0; y < lut.height; ++y)
    {
        for (unsigned int x = 0; x < lut.width; ++x)
            std::cout << "(" << lut.in_distance[y][x] << " ";
        std::cout << std::endl;
    }

    OutputLayer::ArrayType& data = band_name.empty() ?
        map.getGridData() :
        map.getGridData(output_band);
    for (unsigned int y = 0; y < map.getHeight(); ++y)
    {
        for (unsigned int x = 0; x < map.getWidth(); ++x)
        {
            int value = data[y][x];
            if (value == CLASS_OBSTACLE)
            {
                LOG_DEBUG("inspecting around obstacle cell %i %i", x, y);
                lut.markAllRadius(data, map.getWidth(), map.getHeight(), x, y, CLASS_OBSTACLE);
            }
        }
    }

    for (size_t y = 0; y < map.getHeight(); ++y)
    {
        for (size_t x = 0; x < map.getWidth(); ++x)
        {
            if (data[y][x] == 255)
                data[y][x] = CLASS_OBSTACLE;
        }
    }
}

