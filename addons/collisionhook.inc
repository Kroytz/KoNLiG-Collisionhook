#if defined _collisionhook_included
 #endinput
#endif
#define _collisionhook_included

/**
 * Called when the game is performing VPhysics collision checks between two entities.
 * 
 * @param ent1		First entity index.
 * @param ent2		Second entity index.
 * @param result	True if the two entities should collide, false if not.
 * 
 * @return			Plugin_Handled or higher to use 'result' value as the original return value.
 */
forward Action CH_ShouldCollide(int ent1, int ent2, bool &result);

/**
 * Called when the game performs collision checks through trace filters.
 *
 * Note that you should avoid doing expensive computations or declaring large local arrays.
 * 
 * @param ent1		First entity index.
 * @param ent2		Second entity index.
 * @param result	True if the two entities should collide, false if not.
 * 
 * @return			Plugin_Handled or higher to use 'result' value as the original return value.
 */
forward Action CH_PassFilter(int ent1, int ent2, bool &result);

/**
 * Do not edit below this line!
 */
public Extension __ext_collisionhook = 
{
	name = "collisionhook",
	file = "collisionhook.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};