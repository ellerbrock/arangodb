////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Maskings.h"

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::maskings;

MaskingsResult Maskings::fromFile(std::string const& filename) {
  std::string definition;

  try {
    definition = basics::FileUtils::slurp(filename);
  } catch (std::exception const& e) {
    std::string msg =
        "cannot read maskings file '" + filename + "': " + e.what();
    LOG_TOPIC(DEBUG, Logger::CONFIG) << msg;

    return MaskingsResult(MaskingsResult::CANNOT_READ_FILE, msg);
  }

  LOG_TOPIC(DEBUG, Logger::CONFIG) << "found maskings file '" << filename;

  if (definition.empty()) {
    std::string msg = "maskings file '" + filename + "' is empty";
    LOG_TOPIC(DEBUG, Logger::CONFIG) << msg;
    return MaskingsResult(MaskingsResult::CANNOT_READ_FILE, msg);
  }

  std::unique_ptr<Maskings> maskings(new Maskings{});

  try {
    std::shared_ptr<VPackBuilder> parsed =
        velocypack::Parser::fromJson(definition);

    ParseResult<Maskings> res = maskings->parse(parsed->slice());

    if (res.status != ParseResult<Maskings>::VALID) {
      return MaskingsResult(MaskingsResult::ILLEGAL_DEFINITION, res.message);
    }

    return MaskingsResult(std::move(maskings));
  } catch (velocypack::Exception const& e) {
    std::string msg =
        "cannot parse maskings file '" + filename + "': " + e.what();
    LOG_TOPIC(DEBUG, Logger::CONFIG) << msg << ". file content: " << definition;

    return MaskingsResult(MaskingsResult::CANNOT_PARSE_FILE, msg);
  }
}

ParseResult<Maskings> Maskings::parse(VPackSlice const& def) {
  if (!def.isObject()) {
    return ParseResult<Maskings>{ParseResult<Maskings>::DUPLICATE_COLLECTION,
                                 "expecting an object for masking definition",
                                 Maskings()};
  }

  for (auto const& entry : VPackObjectIterator(def, false)) {
    std::string key = entry.key.copyString();
    LOG_TOPIC(TRACE, Logger::CONFIG) << "masking collection '" << key << "'";

    if (_collections.find(key) != _collections.end()) {
      return ParseResult<Maskings>{ParseResult<Maskings>::DUPLICATE_COLLECTION,
                                   "duplicate collection entry '" + key + "'",
                                   Maskings()};
    }

    ParseResult<Collection> c = Collection::parse(entry.value);

    if (c.status != ParseResult<Collection>::VALID) {
      return ParseResult<Maskings>{
          (ParseResult<Maskings>::StatusCode)(int)c.status, c.message,
          Maskings()};
    }

    _collections[key] = c.result;
  }

  return ParseResult<Maskings>{ParseResult<Maskings>::VALID, "", Maskings()};
}

bool Maskings::shouldDumpStructure(std::string const& name) {
  auto const itr = _collections.find(name);

  if (itr == _collections.end()) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "not found";
    return false;
  }

  switch (itr->second.selection()) {
    case CollectionSelection::FULL:
      return true;
    case CollectionSelection::IGNORE:
      return false;
    case CollectionSelection::STRUCTURE:
      return true;
  }
}

bool Maskings::shouldDumpData(std::string const& name) {
  auto const itr = _collections.find(name);

  if (itr == _collections.end()) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "not found";
    return false;
  }

  switch (itr->second.selection()) {
    case CollectionSelection::FULL:
      return true;
    case CollectionSelection::IGNORE:
      return false;
    case CollectionSelection::STRUCTURE:
      return false;
  }
}
