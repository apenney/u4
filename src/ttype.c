/*
 * $Id$
 */

/*
{ "opaque", MASK_OPAQUE },
{ "animated", MASK_ANIMATED },
*/

#include <stddef.h>
#include <string.h>
#include <libxml/xmlmemory.h>

#include "ttype.h"

#include "context.h"
#include "error.h"
#include "list.h"
#include "monster.h"
#include "settings.h"
#include "u4file.h"
#include "xml.h"

/* attr masks */
#define MASK_SHIP               0x0001
#define MASK_HORSE              0x0002
#define MASK_BALLOON            0x0004
#define MASK_DISPEL             0x0008
#define MASK_TALKOVER           0x0010
#define MASK_DOOR               0x0020
#define MASK_LOCKEDDOOR         0x0040
#define MASK_CHEST              0x0080
#define MASK_ATTACKOVER         0x0100
#define MASK_CANLANDBALLOON     0x0200
#define MASK_REPLACEMENT        0x0400

/* movement masks */
#define MASK_SWIMABLE           0x0001
#define MASK_SAILABLE           0x0002
#define MASK_UNFLYABLE          0x0004
#define MASK_MONSTER_UNWALKABLE 0x0008

/* tile values 0-127 */
int tileInfoLoaded = 0;
TileRule *tile_rules = NULL;
int numRules = 0;

ListNode *tilesetList = NULL;  

void tilesetLoad(const char *filename, const char *image, int w, int h, int bpp, CompressionType comp, TilesetType type);
void tileLoadRulesFromXml();
int tileLoadTileInfo(Tile** tiles, int index, xmlNodePtr node);
int ruleLoadProperties(TileRule *rule, xmlNodePtr node);
Tile *tileFindByName(const char *name);
TileRule *ruleFindByName(const char *name);

/**
 * Loads all tilesets using the filename
 * indicated by 'tilesetFilename' as a definition
 */
void tilesetLoadAllTilesetsFromXml(const char *tilesetFilename) {
    if (tileInfoLoaded)
        return;

    tileInfoLoaded = 1;

    tileLoadRulesFromXml();
    tilesetLoad("tiles.xml", "shapes.vga", 16, 16, 8, COMP_NONE, TILESET_BASE);
    tilesetLoad("dungeonTiles.xml", NULL, 0, 0, 0, COMP_NONE, TILESET_DUNGEON);
}

/**
 * Delete all tilesets
 */
void tilesetDeleteAllTilesets() {
    ListNode *node = tilesetList;
    Tileset* tileset;
    
    while(node) {
        tileset = (Tileset*)node->data;
        if (tileset) {
            free(tileset->tiles);
        }
        free(node->data);
        node = node->next;
    }

    listDelete(tilesetList);
    tilesetList = NULL;
}

/**
 * Loads a tileset from the .xml file indicated by 'filename'
 */
void tilesetLoad(const char *filename, const char *image, int w, int h, int bpp, CompressionType comp, TilesetType type) {
    xmlDocPtr doc;
    xmlNodePtr root, node;
    Tileset *tileset;    
    ListNode *list = tilesetList;
    
    /* make sure we aren't loading the same type of tileset twice */
    while (list) {
        if (type == ((Tileset*)list->data)->type)
            errorFatal("error: tileset of type %d already loaded", type);
        list = list->next;
    }
    
    /* open the filename for the tileset and parse it! */
    doc = xmlParse(filename);
    root = xmlDocGetRootElement(doc);
    if (xmlStrcmp(root->name, (const xmlChar *) "tiles") != 0)
        errorFatal("malformed %s", filename);

    tileset = (Tileset *)malloc(sizeof(Tileset));
    if (tileset == NULL)
        errorFatal("error allocating memory for tileset");
    tileset->tileWidth = w;
    tileset->tileHeight = h;
    tileset->bpp = bpp;
    tileset->compType = comp;
    tileset->type = type;
    tileset->numTiles = 0;
    tileset->totalFrames = 0;
    tileset->tileGraphic = NULL;
    tileset->tiles = NULL;

    /* count how many tiles are in the tileset */
    for (node = root->xmlChildrenNode; node; node = node->next) {
        if (xmlNodeIsText(node) || xmlStrcmp(node->name, "tile") != 0)
            continue;
        else tileset->numTiles++;
    }

    if (tileset->numTiles > 0) {
        /* FIXME: eventually, each tile definition won't be duplicated,
           so this will work as it should.  For now, we stick to 256 tiles
        tileset->tiles = (Tile*)malloc(sizeof(Tile) * (tileset->numTiles + 1));
        */
        tileset->tiles = (Tile*)malloc(sizeof(Tile) * 257);

        if (tileset->tiles == NULL)
            errorFatal("error allocating memory for tiles");
        
        for (node = root->xmlChildrenNode; node; node = node->next) {
            if (xmlNodeIsText(node) || xmlStrcmp(node->name, "tile") != 0)
                continue;
            
            tileLoadTileInfo(&tileset->tiles, tileset->totalFrames, node);
            tileset->totalFrames += tileset->tiles[tileset->totalFrames].frames;            
        }
    }
    else errorFatal("Error: no 'tile' nodes defined in %s", filename);    

    /* FIXME: move to tilesetLoadTileImage() function */
    /***/
    if (image) {
        U4FILE *file = u4fopen(image);    
        if (file)
            screenLoadImageVga(&tileset->tileGraphic, w, h * tileset->totalFrames, file, comp);
        u4fclose(file);

        tileset->tileGraphic = screenScale(tileset->tileGraphic, settings->scale, tileset->totalFrames, 1);
    }
    /***/

    tilesetList = listAppend(tilesetList, tileset);
    xmlFree(doc);
}

/**
 * Returns the tileset of the given type, if it is already loaded
 */
Tileset *tilesetGetByType(TilesetType type) {
    ListNode *node;    

    node = tilesetList;
    while (node) {
        if (((Tileset*)node->data)->type == type)
            return (Tileset*)node->data;
        node = node->next;
    }
    
    errorFatal("Tileset of type %d not found", type);
    return NULL;
}

/**
 * 
 */
Tile *tileCurrentTilesetInfo() {
    return (c && c->location) ? c->location->tileset->tiles : tilesetGetByType(TILESET_BASE)->tiles;
}

/**
 * Load tile information from xml.
 */
void tileLoadRulesFromXml() {
    xmlDocPtr doc;
    xmlNodePtr root, node;
    
    numRules = 0;
    if (tile_rules)
        free(tile_rules);

    doc = xmlParse("tileRules.xml");
    root = xmlDocGetRootElement(doc);
    if (xmlStrcmp(root->name, (const xmlChar *) "tileRules") != 0)
        errorFatal("malformed tileRules.xml");

    /* first, we need to count how many rules we are loading */
    for (node = root->xmlChildrenNode; node; node = node->next) {
        if (xmlNodeIsText(node) || xmlStrcmp(node->name, "rule") != 0)
            continue;
        else numRules++;        
    }

    if (numRules == 0)
        return;
    else {
        int i = 0;

        tile_rules = (TileRule *)malloc(sizeof(TileRule) * (numRules + 1));
        if (!tile_rules)
            errorFatal("Error allocating memory for tile rules");

        for (node = root->xmlChildrenNode; node; node = node->next) {
            if (xmlNodeIsText(node) || xmlStrcmp(node->name, "rule") != 0)
                continue;
            
            ruleLoadProperties(&tile_rules[i++], node);
        }
    }

    if (ruleFindByName("default") == NULL)
        errorFatal("no 'default' rule found in tileRules.xml");

    xmlFree(doc);
}

/**
 * Loads tile information from the xml node 'node', if it
 * is a valid tile node.  This loads in both <tile> and 
 * <dngTile> nodes.
 */
int tileLoadTileInfo(Tile** tiles, int index, xmlNodePtr node) {    
    int i;
    Tile tile, *tilePtr;

    /* ignore 'text' nodes */        
    if (xmlNodeIsText(node) || xmlStrcmp(node->name, (xmlChar *)"tile") != 0)
        return 1;
            
    tile.name = xmlGetPropAsStr(node, "name"); /* get the name of the tile */
    tile.index = index; /* get the index of the tile */
    tile.frames = 1;
    tile.animated = xmlGetPropAsBool(node, "animated"); /* see if the tile is animated */
    tile.opaque = xmlGetPropAsBool(node, "opaque"); /* see if the tile is opaque */

    /* get the tile to display for the current tile */
    if (xmlPropExists(node, "displayTile"))
        tile.displayTile = xmlGetPropAsInt(node, "displayTile");
    else
        tile.displayTile = index; /* itself */    

    /* find the rule that applies to the current tile, if there is one.
       if there is no rule specified, it defaults to the "default" rule */
    if (xmlPropExists(node, "rule")) {
        tile.rule = ruleFindByName(xmlGetPropAsStr(node, "rule"));
        if (tile.rule == NULL)
            tile.rule = ruleFindByName("default");
    }
    else tile.rule = ruleFindByName("default");

    /* for each frame of the tile, duplicate our values */    
    if (xmlPropExists(node, "frames"))
        tile.frames = xmlGetPropAsInt(node, "frames");
    
    tilePtr = (*tiles) + index;
    for (i = 0; i < tile.frames; i++) {        
        memcpy(tilePtr + i, &tile, sizeof(Tile));
        (tilePtr + i)->index += i; /* fix the index */        
    }
    
    return 1;
}

/**
 * Load properties for the current rule node 
 */
int ruleLoadProperties(TileRule *rule, xmlNodePtr node) {
    int i;
    
    static const struct {
        const char *name;
        unsigned int mask;        
    } booleanAttributes[] = {        
        { "dispel", MASK_DISPEL },
        { "talkover", MASK_TALKOVER },
        { "door", MASK_DOOR },
        { "lockeddoor", MASK_LOCKEDDOOR },
        { "chest", MASK_CHEST },
        { "ship", MASK_SHIP },
        { "horse", MASK_HORSE },
        { "balloon", MASK_BALLOON },
        { "canattackover", MASK_ATTACKOVER },
        { "canlandballoon", MASK_CANLANDBALLOON },
        { "replacement", MASK_REPLACEMENT }
    };

    static const struct {
        const char *name;
        unsigned int mask;      
    } movementBooleanAttr[] = {        
        { "swimable", MASK_SWIMABLE },
        { "sailable", MASK_SAILABLE },       
        { "unflyable", MASK_UNFLYABLE },       
        { "monsterunwalkable", MASK_MONSTER_UNWALKABLE }        
    };
    static const char *speedEnumStrings[] = { "fast", "slow", "vslow", "vvslow", NULL };
    static const char *effectsEnumStrings[] = { "none", "fire", "sleep", "poison", "poisonField", "electricity", "lava", NULL };

    rule->mask = 0;
    rule->movementMask = 0;
    rule->speed = FAST;
    rule->effect = EFFECT_NONE;
    rule->walkonDirs = MASK_DIR_ALL;
    rule->walkoffDirs = MASK_DIR_ALL;    
    rule->name = xmlGetPropAsStr(node, "name");    

    for (i = 0; i < sizeof(booleanAttributes) / sizeof(booleanAttributes[0]); i++) {
        if (xmlGetPropAsBool(node, booleanAttributes[i].name))
            rule->mask |= booleanAttributes[i].mask;        
    }

    for (i = 0; i < sizeof(movementBooleanAttr) / sizeof(movementBooleanAttr[0]); i++) {
        if (xmlGetPropAsBool(node, movementBooleanAttr[i].name))
            rule->movementMask |= movementBooleanAttr[i].mask;
    }

    if (xmlPropCmp(node, "cantwalkon", "all") == 0)
        rule->walkonDirs = 0;
    else if (xmlPropCmp(node, "cantwalkon", "west") == 0)
        rule->walkonDirs = DIR_REMOVE_FROM_MASK(DIR_WEST, rule->walkonDirs);
    else if (xmlPropCmp(node, "cantwalkon", "north") == 0)
        rule->walkonDirs = DIR_REMOVE_FROM_MASK(DIR_NORTH, rule->walkonDirs);
    else if (xmlPropCmp(node, "cantwalkon", "east") == 0)
        rule->walkonDirs = DIR_REMOVE_FROM_MASK(DIR_EAST, rule->walkonDirs);
    else if (xmlPropCmp(node, "cantwalkon", "south") == 0)
        rule->walkonDirs = DIR_REMOVE_FROM_MASK(DIR_SOUTH, rule->walkonDirs);
    else if (xmlPropCmp(node, "cantwalkon", "advance") == 0)
        rule->walkonDirs = DIR_REMOVE_FROM_MASK(DIR_ADVANCE, rule->walkonDirs);
    else if (xmlPropCmp(node, "cantwalkon", "retreat") == 0)
        rule->walkonDirs = DIR_REMOVE_FROM_MASK(DIR_RETREAT, rule->walkonDirs);

    if (xmlPropCmp(node, "cantwalkoff", "all") == 0)
        rule->walkoffDirs = 0;
    else if (xmlPropCmp(node, "cantwalkoff", "west") == 0)
        rule->walkoffDirs = DIR_REMOVE_FROM_MASK(DIR_WEST, rule->walkoffDirs);
    else if (xmlPropCmp(node, "cantwalkoff", "north") == 0)
        rule->walkoffDirs = DIR_REMOVE_FROM_MASK(DIR_NORTH, rule->walkoffDirs);
    else if (xmlPropCmp(node, "cantwalkoff", "east") == 0)
        rule->walkoffDirs = DIR_REMOVE_FROM_MASK(DIR_EAST, rule->walkoffDirs);
    else if (xmlPropCmp(node, "cantwalkoff", "south") == 0)
        rule->walkoffDirs = DIR_REMOVE_FROM_MASK(DIR_SOUTH, rule->walkoffDirs);
    else if (xmlPropCmp(node, "cantwalkoff", "advance") == 0)
        rule->walkoffDirs = DIR_REMOVE_FROM_MASK(DIR_ADVANCE, rule->walkoffDirs);
    else if (xmlPropCmp(node, "cantwalkoff", "retreat") == 0)
        rule->walkoffDirs = DIR_REMOVE_FROM_MASK(DIR_RETREAT, rule->walkoffDirs);

    rule->speed = xmlGetPropAsEnum(node, "speed", speedEnumStrings);
    rule->effect = xmlGetPropAsEnum(node, "effect", effectsEnumStrings);

    return 1;
}

Tile *tileFindByName(const char *name) {
    /* FIXME: rewrite for new system */
    Tile *tileset = tileCurrentTilesetInfo();
    int i;

    if (!name)
        return NULL;

    for (i = 0; i < 256; i++) {
        if (tileset[i].name == NULL)
            errorFatal("Error: not all tiles have a \"name\" attribute");
            
        if (strcasecmp(name, tileset[i].name) == 0)
            return &tileset[i];
    }

    return NULL;
}

TileRule *ruleFindByName(const char *name) {
    int i;
    
    if (!name)
        return NULL;
    
    for (i = 0; i < numRules; i++)
        if (strcasecmp(name, tile_rules[i].name) == 0)
            return &tile_rules[i];

    return NULL;
}

int tileTestBit(unsigned char tile, unsigned short mask) {
    Tile *tileset = tileCurrentTilesetInfo();
    return (tileset[tile].rule->mask & mask) != 0;
}

int tileTestMovementBit(unsigned char tile, unsigned short mask) {
    Tile *tileset = tileCurrentTilesetInfo();
    return (tileset[tile].rule->movementMask & mask) != 0;
}

int tileCanWalkOn(unsigned char tile, Direction d) {
    Tile *tileset = tileCurrentTilesetInfo();
    return DIR_IN_MASK(d, tileset[tile].rule->walkonDirs);
}

int tileCanWalkOff(unsigned char tile, Direction d) {
    Tile *tileset = tileCurrentTilesetInfo();
    return DIR_IN_MASK(d, tileset[tile].rule->walkoffDirs);
}

int tileCanAttackOver(unsigned char tile) {    
    /* All tiles that you can walk, swim, or sail on, can be attacked over.
       All others must declare themselves */
    return tileIsWalkable(tile) || tileIsSwimable(tile) || tileIsSailable(tile) ||       
        tileTestBit(tile, MASK_ATTACKOVER);
}

int tileCanLandBalloon(unsigned char tile) {
    return tileTestBit(tile, MASK_CANLANDBALLOON);
}

int tileIsReplacement(unsigned char tile) {
    return tileTestBit(tile, MASK_REPLACEMENT);
}

int tileIsWalkable(unsigned char tile) {
    Tile *tileset = tileCurrentTilesetInfo();
    return tileset[tile].rule->walkonDirs > 0;
}

int tileIsMonsterWalkable(unsigned char tile) {
    return !tileTestMovementBit(tile, MASK_MONSTER_UNWALKABLE);
}

int tileIsSwimable(unsigned char tile) {
    return tileTestMovementBit(tile, MASK_SWIMABLE);
}

int tileIsSailable(unsigned char tile) {
    return tileTestMovementBit(tile, MASK_SAILABLE);
}

int tileIsWater(unsigned char tile) {
    return (tileIsSwimable(tile) | tileIsSailable(tile));
}

int tileIsFlyable(unsigned char tile) {
    return !tileTestMovementBit(tile, MASK_UNFLYABLE);
}

int tileIsDoor(unsigned char tile) {
    return tileTestBit(tile, MASK_DOOR);
}

int tileIsLockedDoor(unsigned char tile) {
    return tileTestBit(tile, MASK_LOCKEDDOOR);
}

int tileIsChest(unsigned char tile) {
    return tileTestBit(tile, MASK_CHEST);
}

unsigned char tileGetChestBase() {
    return tileFindByName("chest")->index;
}

int tileIsShip(unsigned char tile) {
    return tileTestBit(tile, MASK_SHIP);
}

unsigned char tileGetShipBase() {
    return tileFindByName("ship")->index;
}

int tileIsPirateShip(unsigned char tile) {
    if (tile >= PIRATE_TILE && tile < (PIRATE_TILE + 4))
        return 1;
    return 0;
}

int tileIsHorse(unsigned char tile) {
    return tileTestBit(tile, MASK_HORSE);
}

unsigned char tileGetHorseBase() {
    return tileFindByName("horse")->index;
}

int tileIsBalloon(unsigned char tile) {
    return tileTestBit(tile, MASK_BALLOON);
}

unsigned char tileGetBalloonBase() {
    return tileFindByName("balloon")->index;
}

int tileCanDispel(unsigned char tile) {
    return tileTestBit(tile, MASK_DISPEL);
}

Direction tileGetDirection(unsigned char tile) {
    if (tileIsShip(tile))
        return (Direction) (tile - tileFindByName("ship")->index + DIR_WEST);
    if (tileIsPirateShip(tile))
        return (Direction) (tile - PIRATE_TILE + DIR_WEST);
    else if (tileIsHorse(tile))
        return tile == tileFindByName("horse")->index ? DIR_WEST : DIR_EAST;
    else
        return DIR_WEST;        /* some random default */
}

int tileSetDirection(unsigned char *tile, Direction dir) {
    int newDir = 1;
    int oldTile = *tile;

    /* Make sure we even have a direction */
    if (dir <= DIR_NONE)
        return 0;

    if (tileIsShip(*tile))
        *tile = tileFindByName("ship")->index + dir - DIR_WEST;
    else if (tileIsPirateShip(*tile))
        *tile = PIRATE_TILE + dir - DIR_WEST;
    else if (tileIsHorse(*tile))
        *tile = (dir == DIR_WEST ? tileFindByName("horse")->index : tileFindByName("horse")->index + 1);
    else   
        newDir = 0;

    if (oldTile == *tile)
        newDir = 0;

    return newDir;
}

int tileCanTalkOver(unsigned char tile) {
    return tileTestBit(tile, MASK_TALKOVER);
}

TileSpeed tileGetSpeed(unsigned char tile) {
    Tile *tileset = tileCurrentTilesetInfo();
    return tileset[tile].rule->speed;
}

TileEffect tileGetEffect(unsigned char tile) {
    Tile *tileset = tileCurrentTilesetInfo();
    return tileset[tile].rule->effect;
}

TileAnimationStyle tileGetAnimationStyle(unsigned char tile) {
    Tile *tileset = tileCurrentTilesetInfo();
   
    if (tileset[tile].animated)
        return ANIM_SCROLL;
    else if (tile == 75)
        return ANIM_CAMPFIRE;
    else if (tile == 10)
        return ANIM_CITYFLAG;
    else if (tile == 11)
        return ANIM_CASTLEFLAG;
    else if (tile == 16)
        return ANIM_WESTSHIPFLAG;
    else if (tile == 18)
        return ANIM_EASTSHIPFLAG;
    else if (tile == 14)
        return ANIM_LCBFLAG;
    else if ((tile >= 32 && tile < 48) ||
             (tile >= 80 && tile < 96) ||
             (tile >= 132 && tile < 144))
        return ANIM_TWOFRAMES;
    else if (tile >= 144)
        return ANIM_FOURFRAMES;

    return ANIM_NONE;
}

void tileAdvanceFrame(unsigned char *tile) {
    TileAnimationStyle style = tileGetAnimationStyle(*tile);

    if (style == ANIM_TWOFRAMES) {
        if ((*tile) % 2)
            (*tile)--;
        else
            (*tile)++;
    }
    else if (style == ANIM_FOURFRAMES) {
        if ((*tile) % 4 == 3)
            (*tile) &= ~(0x3);
        else
            (*tile)++;
    }
}

int tileIsOpaque(unsigned char tile) {
    extern Context *c;
    Tile *tileset = tileCurrentTilesetInfo();

    if (c->opacity)
        return tileset[tile].opaque ? 1 : 0;
    else return 0;
}

unsigned char tileForClass(int klass) {
    return (klass * 2) + 0x20;
}
