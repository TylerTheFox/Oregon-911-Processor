/*
    Author: Brandan Tyler Lasley
    Date:   2/16/2016
*/
#pragma once
#include "data.h"
#include <string>
#include <iterator>
#include <sstream>
#include <vector>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/StreamCopier.h>
#include <Poco/Exception.h>
#include <Poco/String.h>
#include <Poco/Path.h>
#include <Poco/URI.h>
namespace util
{
    // Cycles though all of knownAgencyList until it finds what it wants or returns an empty string.
    inline std::string getAgencyByStation(const std::string & abbv) {
        std::string ret = "UNK";
        bool stop = false;
        int i = 0;
        while (!stop) {
            if (knownAgencyList[i].abbv == abbv) {
                ret = knownAgencyList[i].name;
                stop = true;
            }
            else if (knownAgencyList[i].abbv.empty()) stop = true;
            i++;
        }

        // Last resort SLOW!
        // We couldn't find this agency in the known agency lookup table.
        // Lets check the stations list which has its own copy of stations.
        i = 0;
        stop = false;
        if (ret.empty()) {
            while (!stop) {
                if (knownStationList[i].abbv == abbv) {
                    ret = knownStationList[i].agency;
                    stop = true;
                }
                else if (knownStationList[i].station.empty()) stop = true;
                i++;
            }
        }
        return ret;
    }

    inline std::string http_get(const std::string & url, const std::string & data = "") {
        std::string content;
        try {
            // Initialize session
            Poco::URI uri(url);
            Poco::Net::HTTPClientSession client_session(uri.getHost(), uri.getPort());

            // Prepare and send request
            std::string path(uri.getPathAndQuery());

            Poco::Net::HTTPRequest req(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);

            if (!data.empty()) {
                // Nevermind we want to post.
                req.setMethod(Poco::Net::HTTPRequest::HTTP_POST);

                client_session.setKeepAlive(true);

                // Set Expect 100 continue
                req.set("Expect", "100-continue");

                req.setContentType("application/x-www-form-urlencoded");
                req.setKeepAlive(true); // notice setKeepAlive is also called on client_session (above)
                req.setContentLength(data.length());
                std::ostream& os = client_session.sendRequest(req);

                std::stringstream ss;
                ss << data;
                Poco::StreamCopier::copyStream(ss, os);
            }
            else {
                client_session.sendRequest(req);
            }

            req.set("User-Agent", "Oregon 911 Processor");

            // Get response
            Poco::Net::HTTPResponse res;
            std::istream& is = client_session.receiveResponse(res);

            // Set return string
            content = { std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>() };
        }
        catch (Poco::Exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
        return content;
    }

    inline char getCountyByName(const std::string & county) {

        if (Poco::toLower(county) == "wccca") {
            return 'W';
        }
        else if (Poco::toLower(county) == "ccom") {
            return 'C';
        }
        return 0;
    }

    inline std::vector<struct WCCCA_JSON> getWCCCAGPSFromHTML(const std::string & WCCCA_HTML) {
        std::vector<struct WCCCA_JSON> WCCCA_GPS_DATA;

        // Find GPS Location
        size_t location_start = WCCCA_HTML.rfind("<script type=\"text/javascript\">");
        size_t location_end = WCCCA_HTML.rfind("</script>");
        std::string gps_code = WCCCA_HTML.substr(location_start, location_end - location_start);

        // Reduce further
        location_start = gps_code.find("LoadMarker");
        gps_code = gps_code.substr(location_start, gps_code.length());
        gps_code = gps_code.substr(0, gps_code.find("\n", 0));

        // Ok we reduced enough, lets split the string by ;
        Poco::StringTokenizer rows(gps_code, ";");

        for (int i = 0; i < rows.count(); i++) {
            if (0 == rows[i].find("LoadMarker")) {
                // Ok now on this CPU intensive journey we need to remove "LoadMarker(" and the last ")".
                std::string row = rows[i];

                // "LoadMarker(" is 11 characters long with the -1 for the ")" at the end.
                const int lengthOfLoadMarker = 11;
                row = row.substr(lengthOfLoadMarker, row.length() - lengthOfLoadMarker - 1);

                // Strip
                row.erase(std::remove(row.begin(), row.end(), '\''), row.end());
                row.erase(std::remove(row.begin(), row.end(), ')'), row.end());

                // We need to split it now by commads.
                Poco::StringTokenizer columns(row, ",");

                const int lengthOfparseFloat = 11;
                std::string lat = columns[0].substr(lengthOfparseFloat, columns[0].length());
                std::string lon = columns[1].substr(lengthOfparseFloat, columns[1].length());
                std::string callNumber = columns[3].substr(1, columns[3].length());
                std::string county = columns[4].substr(1, columns[3].length());

                struct WCCCA_JSON gpsData;

                // Header
                gpsData.h.callNumber = stoi(callNumber);
                gpsData.h.county = util::getCountyByName(county);
                gpsData.h.ignoreGC = false;

                // Location
                gpsData.location.lat = stod(lat);
                gpsData.location.lon = stod(lon);

                // misc
                gpsData.callSum = Poco::toUpper(columns[2].substr(1, columns[2].length()));

                WCCCA_GPS_DATA.push_back(gpsData);
            }
        }
        return WCCCA_GPS_DATA;
    }
}