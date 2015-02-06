#include <cmath>
#include <limits>

#include "bbox.hpp"

BBox::BBox() : m_min(), m_max() { }
BBox::BBox(Point min, Point max) : m_min(min), m_max(max) { }
BBox::BBox(const BBox& other) : m_min(other.min()), m_max(other.max()) { }

Point BBox::min() const { return m_min; }
Point BBox::max() const { return m_max; }
Point BBox::mid() const
{
    return Point(
            m_min.x + (m_max.x - m_min.x) / 2.0,
            m_min.y + (m_max.y - m_min.y) / 2.0);
}

void BBox::min(const Point& p) { m_min = p; }
void BBox::max(const Point& p) { m_max = p; }

bool BBox::overlaps(const BBox& other) const
{
    Point middle(mid());
    Point otherMiddle(other.mid());

    return
        std::abs(middle.x - otherMiddle.x) <
            width() / 2.0  + other.width() / 2.0 &&
        std::abs(middle.y - otherMiddle.y) <
            height() / 2.0 + other.height() / 2.0;
}

bool BBox::contains(const Point& p) const
{
    return p.x >= m_min.x && p.y >= m_min.y && p.x < m_max.x && p.y < m_max.y;
}

bool BBox::contains(const BBox& other) const
{
    return
        m_min.x <= other.m_min.x && m_max.x >= other.m_max.x &&
        m_min.y <= other.m_min.y && m_max.y >= other.m_max.y;
}

double BBox::width()  const { return m_max.x - m_min.x; }
double BBox::height() const { return m_max.y - m_min.y; }

BBox BBox::getNw() const
{
    const Point middle(mid());
    return BBox(Point(m_min.x, middle.y), Point(middle.x, m_max.y));
}

BBox BBox::getNe() const
{
    const Point middle(mid());
    return BBox(Point(middle.x, middle.y), Point(m_max.x, m_max.y));
}

BBox BBox::getSw() const
{
    const Point middle(mid());
    return BBox(Point(m_min.x, m_min.y), Point(middle.x, middle.y));
}

BBox BBox::getSe() const
{
    const Point middle(mid());
    return BBox(Point(middle.x, m_min.y), Point(m_max.x, middle.y));
}

BBox BBox::encapsulate() const
{
    return BBox(
            m_min,
            Point(
                m_max.x + std::numeric_limits<double>::min(),
                m_max.y + std::numeric_limits<double>::min()));
}

Json::Value BBox::toJson() const
{
    Json::Value json;
    json.append(m_min.x);
    json.append(m_min.y);
    json.append(m_max.x);
    json.append(m_max.y);
    return json;
}

BBox BBox::fromJson(const Json::Value& json)
{
    return BBox(
            Point(
                json.get(Json::ArrayIndex(0), 0).asDouble(),
                json.get(Json::ArrayIndex(1), 0).asDouble()),
            Point(
                json.get(Json::ArrayIndex(2), 0).asDouble(),
                json.get(Json::ArrayIndex(3), 0).asDouble()));
}


