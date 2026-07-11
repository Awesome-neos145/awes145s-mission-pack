// WL_AGENT.C

#include "WL_DEF.H"
#pragma hdrstop


/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

#define MAXMOUSETURN	10


#define MOVESCALE		150l
#define BACKMOVESCALE	100l
#define ANGLESCALE		20

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/



//
// player state info
//
boolean		running;
long		thrustspeed;

unsigned	plux,pluy;			// player coordinates scaled to unsigned

int			anglefrac;
int			gotgatgun;	// JR

objtype		*LastAttacker;

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/
// my fucking dreamcast VMU keeps dying
// i can't say the same about my sony pocketstation
// that thing lasted me 6 months before conking out

void	T_Player (objtype *ob);
void	T_Attack (objtype *ob);
void	WarpPlayer (int facedir);
void	InstWarpPlayer (int facedir);
void	SpearBaseWarp (void);

statetype s_player = {false,0,0,T_Player,NULL,NULL};
statetype s_attack = {false,0,0,T_Attack,NULL,NULL};


long	playerxmove,playerymove;

struct atkinf
{
	char	tics,attack,frame;		// attack is 1 for gun, 2 for knife
} attackinfo[6][14] =

{
{ {6,0,1},{6,2,2},{6,0,3},{6,-1,4} }, // knife
{ {6,0,1},{6,1,2},{6,0,3},{6,-1,4} }, // pistol
{ {5,0,1},{5,1,2},{4,3,3},{5,-1,4} }, // mach-gun
{ {5,0,1},{5,1,2},{5,4,3},{5,-1,4} }, // gatling gun
{ {6,0,1},{5,1,2},{5,4,3},{6,-1,4} }, // flamethrower
{ {6,0,1},{6,1,2},{6,0,3},{6,-1,4} }, // rocket launcher
};
// tweaked weapon speeds so i feel better about adding 2 more weapons

int	strafeangle[9] = {0,90,180,270,45,135,225,315,0};

void DrawWeapon (void);
void GiveWeapon (int weapon);
void	GiveAmmo (int ammo);
void RocketAttack (void);
void FlamethrowerAttack (void);

extern	statetype s_rocket;
extern	statetype s_fire1;

//===========================================================================

//----------

void Attack (void);
void Use (void);
void Search (objtype *ob);
void SelectWeapon (void);
void SelectItem (void);

//----------

boolean TryMove (objtype *ob);
void T_Player (objtype *ob);

void ClipMove (objtype *ob, long xmove, long ymove);

/*
=============================================================================

						CONTROL STUFF

=============================================================================
*/

/*
======================
=
= CheckWeaponChange
=
= Keys 1-4 change weapons
=
======================
*/

void CheckWeaponChange (void)
{
	int	i,buttons;

	// no ammo and no rocketlauncher or flamethrower means you absolutely need to use the knife
	if (!gamestate.ammo && gamestate.weapon != wp_rocket && gamestate.weapon != wp_flamethrower)
	{
		if (!gamestate.hasweapon[wp_rocket] && !gamestate.hasweapon[wp_flamethrower])
			return;
	}

	for (i=wp_knife ; i<NUMWEAPONS ; i++)
		if (gamestate.hasweapon[i])
			if (buttonstate[bt_readyknife+i-wp_knife])
			{
				gamestate.weapon = gamestate.chosenweapon = i;
				DrawWeapon ();
				DrawAmmo ();
				return;
			}
}


/*
=======================
=
= ControlMovement
=
= Takes controlx,controly, and buttonstate[bt_strafe]
=
= Changes the player's angle and position
=
= There is an angle hack because when going 70 fps, the roundoff becomes
= significant
=
= also id, 70fps breaks a shit ton of stuff :P
=
=======================
*/

void ControlMovement (objtype *ob)
{
	long	oldx,oldy;
	int		angle,maxxmove;
	int		angleunits;
	long	speed;

	thrustspeed = 0;

	oldx = player->x;
	oldy = player->y;

//
// side to side move
//
	if (buttonstate[bt_strafe])
	{
	//
	// strafing
	//
	//
		if (controlx > 0)
		{
			angle = ob->angle - ANGLES/4;
			if (angle < 0)
				angle += ANGLES;
			Thrust (angle,controlx*MOVESCALE);	// move to left
		}
		else if (controlx < 0)
		{
			angle = ob->angle + ANGLES/4;
			if (angle >= ANGLES)
				angle -= ANGLES;
			Thrust (angle,-controlx*MOVESCALE);	// move to right
		}
	}
	else
	{
	//
	// not strafing
	//
		anglefrac += controlx;
		angleunits = anglefrac/ANGLESCALE;
		anglefrac -= angleunits*ANGLESCALE;
		ob->angle -= angleunits;

		if (ob->angle >= ANGLES)
			ob->angle -= ANGLES;
		if (ob->angle < 0)
			ob->angle += ANGLES;

	}

//
// forward/backwards move
//
	if (controly < 0)
	{
		Thrust (ob->angle,-controly*MOVESCALE);	// move forwards
	}
	else if (controly > 0)
	{
		angle = ob->angle + ANGLES/2;
		if (angle >= ANGLES)
			angle -= ANGLES;
		Thrust (angle,controly*BACKMOVESCALE);		// move backwards
	}

	if (gamestate.victoryflag)		// watching the BJ actor
		return;

//
// calculate total move
//
	playerxmove = player->x - oldx;
	playerymove = player->y - oldy;
}

/*
=============================================================================

					STATUS WINDOW STUFF

=============================================================================
*/


/*
==================
=
= StatusDrawPic
=
==================
*/

void StatusDrawPic (unsigned x, unsigned y, unsigned picnum)
{
	unsigned	temp;

	temp = bufferofs;
	bufferofs = 0;

	bufferofs = PAGE1START+(200-STATUSLINES)*SCREENWIDTH;
	LatchDrawPic (x,y,picnum);
	bufferofs = PAGE2START+(200-STATUSLINES)*SCREENWIDTH;
	LatchDrawPic (x,y,picnum);
	bufferofs = PAGE3START+(200-STATUSLINES)*SCREENWIDTH;
	LatchDrawPic (x,y,picnum);

	bufferofs = temp;
}


/*
==================
=
= DrawFace
=
==================
*/

void DrawFace (void)
{
	int displayhealth = gamestate.health;
	if (displayhealth > 100)
		displayhealth = 100;
	
	if (gamestate.health)
	{
		#ifdef SPEAR
		if (godmode)
			StatusDrawPic (17,4,GODMODEFACE1PIC+gamestate.faceframe);
		else
		#endif
		StatusDrawPic (17,4,FACE1APIC+3*((100-displayhealth)/16)+gamestate.faceframe);
	}
	else
	{
#ifndef SPEAR
	 if (LastAttacker->obclass == needleobj)
	   StatusDrawPic (17,4,FACE8APIC); // TEXAS? HOLY COW, YOU KNOW WHAT COMES FROM TEXAS DONTCHA?
	 else
#endif
	   StatusDrawPic (17,4,FACE8APIC);
	}
}


/*
===============
=
= UpdateFace
=
= Calls draw face if time to change
=
===============
*/

#define FACETICS	70

int	facecount;

void	UpdateFace (void)
{

	if (SD_SoundPlaying() == GETGATLINGSND)
	  return;

	facecount += tics;
	if (facecount > US_RndT())
	{
		gamestate.faceframe = (US_RndT()>>6);
		if (gamestate.faceframe==3)
			gamestate.faceframe = 1;

		facecount = 0;
		DrawFace ();
	}
}



/*
===============
=
= LatchNumber
=
= right justifies and pads with blanks
=
===============
*/

void	LatchNumber (int x, int y, int width, long number)
{
	unsigned	length,c;
	char	str[20];

	ltoa (number,str,10);

	length = strlen (str);

	while (length<width)
	{
		StatusDrawPic (x,y,N_BLANKPIC);
		x++;
		width--;
	}

	c= length <= width ? 0 : length-width;

	while (c<length)
	{
		StatusDrawPic (x,y,str[c]-'0'+ N_0PIC);
		x++;
		c++;
	}
}


/*
===============
=
= DrawHealth
=
===============
*/

void	DrawHealth (void)
{
	LatchNumber (21,16,3,gamestate.health);
	// hahah funny story
	// i don't need to rely on displayhealth at ALL for this function
	// drawing faces and drawing health are two totally different things
	// only now am i getting my head outta my ass
}


/*
===============
=
= TakeDamage
=
===============
*/

void	TakeDamage (int points,objtype *attacker)
{
	int displayhealth = gamestate.health;
	if (displayhealth > 100)
		displayhealth = 100;
	
	LastAttacker = attacker;

	if (gamestate.victoryflag)
		return;
	if (gamestate.difficulty==gd_baby)
	  points>>=2;

	if (!godmode)
		gamestate.health -= points;

	if (gamestate.health<=0)
	{
		gamestate.health = 0;
		playstate = ex_died;
		killerobj = attacker;
	}

	StartDamageFlash (points);

	gotgatgun=0;

	DrawHealth ();
	// pain picture draw
	if (gamestate.health <= 0)
		StatusDrawPic (17,4,FACE8APIC);
	else
		StatusDrawPic (17,4,PAIN1PIC+((100-displayhealth)/16));
	facecount = 0;
	// ...and have him like this for a good split second
	// MAKE BJ'S EYES BUG IF MAJOR DAMAGE!
	//
	#ifdef SPEAR
	if (points > 30 && gamestate.health!=0 && !godmode)
	{
		StatusDrawPic (17,4,BJOUCHPIC);
		facecount = 0;
	}
	#endif

}


/*
===============
=
= HealSelf
=
===============
*/

void	HealSelf (int points)
{
	gamestate.health += points;
	if (gamestate.health>gamestate.maxhealth)
		gamestate.health = gamestate.maxhealth;

	DrawHealth ();
	gotgatgun = 0;	// JR
	DrawFace ();
}


//===========================================================================


/*
===============
=
= DrawLevel
=
===============
*/

void	DrawLevel (void)
{
#ifdef SPEAR
	if (gamestate.mapon == 20)
		LatchNumber (2,16,2,18);
	else
#endif
	LatchNumber (2,16,2,gamestate.mapon+1);
}

//===========================================================================


/*
===============
=
= DrawLives
=
===============
*/

void	DrawLives (void)
{
	LatchNumber (14,16,1,gamestate.lives);
}


/*
===============
=
= GiveExtraMan
=
===============
*/

void	GiveExtraMan (void)
{
	if (gamestate.lives<9)
		gamestate.lives++;
	DrawLives ();
	SD_PlaySound (BONUS1UPSND);
}

//===========================================================================

/*
===============
=
= DrawScore
=
===============
*/

void	DrawScore (void)
{
	LatchNumber (6,16,6,gamestate.score);
}

/*
===============
=
= GivePoints
=
===============
*/

void	GivePoints (long points)
{
	gamestate.score += points;
	while (gamestate.score >= gamestate.nextextra)
	{
		gamestate.nextextra += EXTRAPOINTS;
		GiveExtraMan ();
	}
	DrawScore ();
}

//===========================================================================

/*
==================
=
= DrawWeapon
=
==================
*/

void DrawWeapon (void)
{
	int pic;
	switch (gamestate.weapon)
	{
	case wp_knife:
		pic = KNIFEPIC;
		break;
	case wp_pistol:
		pic = GUNPIC;
		break;
	case wp_machinegun:
		pic = MACHINEGUNPIC;
		break;
	case wp_chaingun:
		pic = GATLINGGUNPIC;
		break;
	case wp_rocket:
		pic = MUTANTBJPIC;	// rocket launcher icon
		break;
	case wp_flamethrower:
		pic = PAUSEDPIC;	// flamethrower icon (placeholder)
		break;
	}
	StatusDrawPic (32,8,pic);
}


/*
==================
=
= DrawKeys
=
==================
*/

void DrawKeys (void)
{
	if (gamestate.keys & 1)
		StatusDrawPic (30,4,GOLDKEYPIC);
	else
		StatusDrawPic (30,4,NOKEYPIC);

	if (gamestate.keys & 2)
		StatusDrawPic (30,20,SILVERKEYPIC);
	else
		StatusDrawPic (30,20,NOKEYPIC);
}



/*
==================
=
= GiveWeapon
=
==================
*/

void GiveWeapon (int weapon)
{
	if (weapon == wp_rocket)
		;	// rockets give their own ammo
	else if (weapon == wp_flamethrower)
		;	// flamethrower gives its own fuel
	else
		GiveAmmo (6);

	gamestate.hasweapon[weapon] = true;
	
	// in mathematical terms, if weapon<gamestate.chosenweapon, we don't switch
	// this does work, but the fact is that if you have a rocketlauncher and pickup a flamethrower,
	// even though it's a new weapon, it won't switch to that
	// oh well lmao
	if (weapon > gamestate.weapon)
	{
		gamestate.weapon = gamestate.chosenweapon = weapon;
		DrawWeapon ();
	}
}


//===========================================================================

/*
===============
=
= DrawAmmo
=
===============
*/

// awes145's editorial
// this code decides what ammo number to display based on the weapon selected
// flamethrower is selected, it shows fuel
// rocket launcher shows... rockets
// otherwise it shows regular ammo
// i will mention this however: there is a bug where running out of bullets will switch to the knife
// and still show the other (?) ammo counts
// i don't know because this game is fucking crashing
void	DrawAmmo (void)
{
	if (gamestate.weapon == wp_rocket)
		LatchNumber (26,16,3,gamestate.rockets);
	else if (gamestate.weapon == wp_flamethrower)
		LatchNumber (26,16,3,gamestate.fuel);
	else
		LatchNumber (26,16,3,gamestate.ammo);
}


/*
===============
=
= GiveAmmo
=
===============
*/

void	GiveAmmo (int ammo)
{
	if (!gamestate.ammo)				// knife was out
	{
		if (!gamestate.attackframe)
		{
			gamestate.weapon = gamestate.chosenweapon;
			DrawWeapon ();
		}
	}
	// what this is meant to do is keep the ammo at 199 if you don't have the bandolier
	// the bandolier just sets the new gamestate.maxammo to 299
	// this is reset during death counts, as i'm not trying to be so generous as the folks at MacPlay/Interplay/whatever and whoever the fuck did the ports with this in
	// also rest in peace rebecca heineman
	gamestate.ammo += ammo;
	if (gamestate.ammo > gamestate.maxammo)
		gamestate.ammo = gamestate.maxammo;
	DrawAmmo ();
}

//===========================================================================

/*
==================
=
= GiveKey
=
==================
*/

void GiveKey (int key)
{
	gamestate.keys |= (1<<key);
	DrawKeys ();
}



/*
=============================================================================

							MOVEMENT

=============================================================================
*/


/*
===================
=
= GetBonus
=
===================
*/
// very clearly added a few msgs
void GetBonus (statobj_t *check)
{
	switch (check->itemnumber)
	{
	case	bo_firstaid:
		if (gamestate.health >= gamestate.maxhealth)
			return;

		SD_PlaySound (HEALTH2SND);
		HealSelf (25);
		GetMessage("Used a First Aid Kit.");
		break;

	case	bo_key1:
	case	bo_key2:
	case	bo_key3:
	case	bo_key4:
		GiveKey (check->itemnumber - bo_key1);
		SD_PlaySound (GETKEYSND);
		GetMessage("Got a key.");
		break;

	case	bo_cross:
		SD_PlaySound (BONUS1SND);
		GivePoints (100);
		gamestate.treasurecount++;
		GetMessage("Picked up a cross.");
		break;
	case	bo_chalice:
		SD_PlaySound (BONUS2SND);
		GivePoints (500);
		gamestate.treasurecount++;
		GetMessage("Picked up a chalice.");
		break;
	case	bo_bible:
		SD_PlaySound (BONUS3SND);
		GivePoints (1000);
		gamestate.treasurecount++;
		GetMessage("Picked up a gold chest.");
		break;
	case	bo_crown:
		SD_PlaySound (BONUS4SND);
		GivePoints (5000);
		gamestate.treasurecount++;
		GetMessage("Picked up a crown.");
		break;

	case	bo_clip:
		if (gamestate.ammo == gamestate.maxammo)
			return;

		SD_PlaySound (GETAMMOSND);
		GiveAmmo (8);
		GetMessage("Got a magazine.");
		break;
	case	bo_clip2:
		if (gamestate.ammo == gamestate.maxammo)
			return;
			// so what id intended you to start with was the knife
			// however to line up with MY STORY, i changed this to the knife
			// so in short, you knife the guards and pick up the ammo and get a pistol
			// i should also make a pistol sprite but whatevs man
			// i'm not here to stress myself out even more
		SD_PlaySound (GETAMMOSND);
		GiveAmmo (4);
		if (!gamestate.hasweapon[wp_pistol])  
			GiveWeapon(wp_pistol);
		GetMessage("Got a used magazine.");
		break;

	case	bo_25clip:
		if (gamestate.ammo == gamestate.maxammo)
		  return;

#ifdef SPEAR
		SD_PlaySound (GETAMMOBOXSND);
#else
		SD_PlaySound (GETAMMOBOXSND);
#endif
		// really trying to make this engine like the mac lmao
		// ...of course with my own engine quirks
		// minus the improved graphics ofc
		GiveAmmo (25);
		GetMessage("Got an ammobox."); 
		break;

	case	bo_machinegun:
		SD_PlaySound (GETMACHINESND);
		GiveWeapon (wp_machinegun);
		GetMessage("Got a Machine Gun.");
		DrawAmmo ();
		break;
	case	bo_chaingun:
		SD_PlaySound (GETGATLINGSND);
		GiveWeapon (wp_chaingun);

		StatusDrawPic (17,4,GOTGATLINGPIC);
		facecount = 0;
		gotgatgun = 1;
		GetMessage("Got a Gatling Gun!");
		DrawAmmo ();
		break;

	case	bo_rocket:
		SD_PlaySound (GETROCKETSND);
		GiveWeapon (wp_rocket);
		gamestate.rockets += 4;
		if (gamestate.rockets > gamestate.maxrockets)
			gamestate.rockets = gamestate.maxrockets;
		StatusDrawPic (17,4,GOTGATLINGPIC);
		facecount = 0;
		GetMessage("Got a Rocket Launcher!"); // now i am become death, the destroyer of worlds
		DrawAmmo ();
		break;

	case	bo_rocketammo:
		if (gamestate.rockets == gamestate.maxrockets)
			return;

		SD_PlaySound (GETAMMOBOXSND);
		gamestate.rockets += 8;
		if (gamestate.rockets > gamestate.maxrockets)
			gamestate.rockets = gamestate.maxrockets;
		GetMessage("Got a rocket box."); // not making singular rockets like in DOOM
		DrawAmmo ();
		break;
	case	bo_fuel:
		if (gamestate.fuel == 99)
			return;

		SD_PlaySound (SLURPIESND); // pouring fuel into the flamethrower (i'm pretty sure you'd just hook the can into the flamethrower but it REALLY doesn't matter)
		gamestate.fuel += 5;
		if (gamestate.fuel > 99)
			gamestate.fuel = 99;
		GetMessage("Got a fuel can.");
		DrawAmmo ();
		break;
	case	bo_flamethrower:
		GiveWeapon (wp_flamethrower);
		gamestate.fuel += 5;
		if (gamestate.fuel > 99)
			gamestate.fuel = 99;
		SD_PlaySound (GETROCKETSND);
		StatusDrawPic (17,4,GOTGATLINGPIC);
		facecount = 0;
		GetMessage("Got a Flamethrower!"); // hot hot HOT
		DrawAmmo ();
		break;
	case	bo_fullheal:
		SD_PlaySound (BONUS1UPSND);
		HealSelf (99);
		GiveAmmo (25);
		GiveExtraMan ();
		gamestate.treasurecount++;
		GetMessage("Extra Life!"); // woohoo!
		break;

	case	bo_food:
		if (gamestate.health >= gamestate.maxhealth)
			return;

		SD_PlaySound (HEALTH1SND);
		HealSelf (10);
		GetMessage("Used a stimpack."); // not really food anymore as i've replaced the sprite
		break;

	case	bo_alpo:
		if (gamestate.health >= gamestate.maxhealth)
			return;

		SD_PlaySound (HEALTH1SND);
		HealSelf (4);
		GetMessage("Ate some dog food."); // ewww
		break;

	case	bo_gibs:
		if (gamestate.health >= gamestate.maxhealth)
			return;
		if (gamestate.health >10)
			return;

		SD_PlaySound (SLURPIESND);
		HealSelf (1);
		GetMessage("Feasted on a dead body."); // good god bro
		break;

		case	bo_bandolier:
		if (gamestate.maxammo == 299)
			return;

		SD_PlaySound (GETAMMOBOXSND);
		gamestate.maxammo = 299;
		if (gamestate.ammo > gamestate.maxammo)
			gamestate.ammo = gamestate.maxammo;
		GiveAmmo (12); // gotta give the user something lol
		DrawAmmo ();
		GetMessage("Got a bandolier!");
		break;

	case	bo_healthpack:
		if (gamestate.maxhealth == 200)
			return;

		SD_PlaySound (GETHLTBCKSND);
		gamestate.maxhealth = 200;
		if (gamestate.health > gamestate.maxhealth)
			gamestate.health = gamestate.maxhealth;
		else
			gamestate.health = gamestate.maxhealth; // fill to new cap
		DrawHealth ();
		GetMessage("Got a health backpack!"); // for perspective you can have up to 4 firstaids, 10 stimpacks, and 25 dogfood from 100HP.
		break;
// i would leave secret schematics in the game's pictures but since CA_FarRead doesn't like reading anything over 64K...
	case	bo_godorb:
		godmode = 1;
		gamestate.godmode = true;
		gamestate.godmodecount++;
		gamestate.health = 100;
		DrawHealth ();
		DrawFace ();
		SD_PlaySound (BONUS1UPSND);
		GetMessage("Got the Spear of Destiny!"); // if id Software's "Spear of Destiny" and FormGen's 2 mission packs for it didn't prove that B.J. was worthy of weilding it, idk what will
		break;

	case	bo_usedfuel:
		if (gamestate.fuel == 99)
			return;
		SD_PlaySound (SLURPIESND);
		gamestate.fuel += 2;
		if (gamestate.fuel > 99)
			gamestate.fuel = 99;
		GetMessage("Got a used fuel can.");
		DrawAmmo ();
		break;

	case	bo_spear:
		spearflag = true;
		spearx = player->x;
		speary = player->y;
		spearangle = player->angle;
		playstate = ex_completed;
	}

	StartBonusFlash ();
	check->shapenum = -1;			// remove from list
}


/*
===================
=
= TryMove
=
= returns true if move ok
= debug: use pointers to optimize
===================
*/

boolean TryMove (objtype *ob)
{
	int			xl,yl,xh,yh,x,y;
	objtype		*check;
	long		deltax,deltay;

	xl = (ob->x-PLAYERSIZE) >>TILESHIFT;
	yl = (ob->y-PLAYERSIZE) >>TILESHIFT;

	xh = (ob->x+PLAYERSIZE) >>TILESHIFT;
	yh = (ob->y+PLAYERSIZE) >>TILESHIFT;

//
// check for solid walls
//
	for (y=yl;y<=yh;y++)
		for (x=xl;x<=xh;x++)
		{
			check = actorat[x][y];
			if (check && check<objlist)
				return false;
		}

//
// check for actors
//
	if (yl>0)
		yl--;
	if (yh<MAPSIZE-1)
		yh++;
	if (xl>0)
		xl--;
	if (xh<MAPSIZE-1)
		xh++;

	for (y=yl;y<=yh;y++)
		for (x=xl;x<=xh;x++)
		{
			check = actorat[x][y];
			if (check > objlist
			&& (check->flags & FL_SHOOTABLE) )
			{
				deltax = ob->x - check->x;
				if (deltax < -MINACTORDIST || deltax > MINACTORDIST)
					continue;
				deltay = ob->y - check->y;
				if (deltay < -MINACTORDIST || deltay > MINACTORDIST)
					continue;

				return false;
			}
		}

	return true;
}


/*
===================
=
= ClipMove
=
===================
*/

void ClipMove (objtype *ob, long xmove, long ymove)
{
	long	basex,basey;

	basex = ob->x;
	basey = ob->y;

	ob->x = basex+xmove;
	ob->y = basey+ymove;
	if (TryMove (ob))
		return;

	if (noclip && ob->x > 2*TILEGLOBAL && ob->y > 2*TILEGLOBAL &&
	ob->x < (((long)(mapwidth-1))<<TILESHIFT)
	&& ob->y < (((long)(mapheight-1))<<TILESHIFT) )
		return;		// walk through walls
		// technically this code is in spear, but disabled anyway
		// the walls draw ugly when you walk through them too...
		// however seeing this is the point as one of my levels idk if my point is valid

	ob->x = basex+xmove;
	ob->y = basey;
	if (TryMove (ob))
		return;

	ob->x = basex;
	ob->y = basey+ymove;
	if (TryMove (ob))
		return;

	ob->x = basex;
	ob->y = basey;
}

//==========================================================================

/*
===================
=
= VictoryTile
=
===================
*/

void VictoryTile (void)
{
#ifndef SPEAR
	SpawnBJVictory ();
#endif

	gamestate.victoryflag = true;
}


/*
===================
=
= Thrust
=
===================
*/

void Thrust (int angle, long speed)
{
	long xmove,ymove;
	long	slowmax;
	unsigned	offset;


	//
	// ZERO FUNNY COUNTER IF MOVED!
	//
	#ifdef SPEAR
	if (speed)
		funnyticount = 0;
	#endif

	thrustspeed += speed;
//
// moving bounds speed
//
	if (speed >= MINDIST*2)
		speed = MINDIST*2-1;

	xmove = FixedByFrac(speed,costable[angle]);
	ymove = -FixedByFrac(speed,sintable[angle]);

	ClipMove(player,xmove,ymove);

	player->tilex = player->x >> TILESHIFT;		// scale to tile values
	player->tiley = player->y >> TILESHIFT;

	offset = farmapylookup[player->tiley]+player->tilex;
	player->areanumber = *(mapsegs[0] + offset) -AREATILE;

	if (*(mapsegs[1] + offset) == EXITTILE)
		VictoryTile ();
	if (*(mapsegs[1] + offset) == WARPEASTTILE)
		WarpPlayer (EAST);
	if (*(mapsegs[1] + offset) == WARPWESTTILE)
		WarpPlayer (WEST);
	if (*(mapsegs[1] + offset) == WARPNORTHTILE)
		WarpPlayer (NORTH);
	if (*(mapsegs[1] + offset) == WARPSOUTHTILE)
		WarpPlayer (SOUTH);
	if (*(mapsegs[1] + offset) == TRUCKONETILE)
		InstWarpPlayer (WARONE);
	if (*(mapsegs[1] + offset) == TRUCKTWOTILE)
		InstWarpPlayer (WARTWO);
	if (*(mapsegs[1] + offset) == TRUCKTHREETILE)
		InstWarpPlayer (WARTHR);
	if (*(mapsegs[1] + offset) == TRUCKFOURTILE)
		InstWarpPlayer (WARFOU);
	if (*(mapsegs[1] + offset) == SPEARTELETILE) // i sometimes wonder if this is the main culprit for the freezing
	{
		spearx = player->x;
		speary = player->y;
		spearangle = player->angle;
		SpearBaseWarp ();
	}
}


/*
=============================================================================

								ACTIONS

=============================================================================
*/

// no i'm not playing on my phone i'm taking care of business
/*
===============
=
= Cmd_Fire
=
===============
*/

void Cmd_Fire (void)
{
	buttonheld[bt_attack] = true;

	gamestate.weaponframe = 0;

	player->state = &s_attack;

	gamestate.attackframe = 0;
	gamestate.attackcount =
		attackinfo[gamestate.weapon][gamestate.attackframe].tics;
	gamestate.weaponframe =
		attackinfo[gamestate.weapon][gamestate.attackframe].frame;
}

//===========================================================================

/*
===============
=
= Cmd_Use
=
===============
*/

void Cmd_Use (void)
{
	objtype 	*check;
	int			checkx,checky,doornum,dir;
	boolean		elevatorok;


//
// find which cardinal direction the player is facing
//
	if (player->angle < ANGLES/8 || player->angle > 7*ANGLES/8)
	{
		checkx = player->tilex + 1;
		checky = player->tiley;
		dir = di_east;
		elevatorok = true;
	}
	else if (player->angle < 3*ANGLES/8)
	{
		checkx = player->tilex;
		checky = player->tiley-1;
		dir = di_north;
		elevatorok = false;
	}
	else if (player->angle < 5*ANGLES/8)
	{
		checkx = player->tilex - 1;
		checky = player->tiley;
		dir = di_west;
		elevatorok = true;
	}
	else
	{
		checkx = player->tilex;
		checky = player->tiley + 1;
		dir = di_south;
		elevatorok = false;
	}

	doornum = tilemap[checkx][checky];
	if (*(mapsegs[1]+farmapylookup[checky]+checkx) == PUSHABLETILE)
	{
	//
	// pushable wall
	//

		PushWall (checkx,checky,dir);
		return;
	}
	if (!buttonheld[bt_use] && doornum == ELEVATORTILE && elevatorok)
	{
	//
	// use elevator
	//
		buttonheld[bt_use] = true;

		tilemap[checkx][checky]++;		// flip switch
		if (*(mapsegs[0]+farmapylookup[player->tiley]+player->tilex) == ALTELEVATORTILE)
			playstate = ex_secretlevel;
		else
			playstate = ex_completed;
		SD_PlaySound (LEVELDONESND);
		SD_WaitSoundDone();
	}
		else if (!buttonheld[bt_use] && doornum & 0x80)
	{
		buttonheld[bt_use] = true;
		OperateDoor (doornum & ~0x80);
	}
	else if (!buttonheld[bt_use] && doornum >= SWITCHTILE) 
   { 
   	buttonheld[bt_use] = true; 
		if (doornum >= SWITCHTILE && doornum <= SWITCHTILE+1)
		{
			switch (doornum)	// Flip the switch
			{
				case SWITCHTILE:
					tilemap[checkx][checky]++;
					break;
				case SWITCHTILE+1:
					tilemap[checkx][checky]--;
					break;
			}
			switch (MAPSPOT(checkx,checky,1))  // activate/deactivate item 
			{
				case 700: // Pushwall North
					PushWall (pwx,pwy,di_north);
					break;
				case 701: // Pushwall South
					PushWall (pwx,pwy,di_south);
					break;
				case 702: // Pushwall East
					PushWall (pwx,pwy,di_east);
					break;
				case 703: // Pushwall West
					PushWall (pwx,pwy,di_west);
					break;
				case 708: // Pushwall2 North
					PushWall (pw2x,pw2y,di_north);
					break;
				case 709: // Pushwall2 South
					PushWall (pw2x,pw2y,di_south);
					break;
				case 710: // Pushwall2 East
					PushWall (pw2x,pw2y,di_east);
					break;
				case 711: // Pushwall2 West
					PushWall (pw2x,pw2y,di_west);
					break;
				case 716: // Pushwall3 North
					PushWall (pw3x,pw3y,di_north);
					break;
				case 717: // Pushwall3 South
					PushWall (pw3x,pw3y,di_south);
					break;
				case 718: // Pushwall3 East
					PushWall (pw3x,pw3y,di_east);
					break;
				case 719: // Pushwall3 West
					PushWall (pw3x,pw3y,di_west);
					break;
				case 724: // Pushwall4 North
					PushWall (pw4x,pw4y,di_north);
					break;
				case 725: // Pushwall4 South
					PushWall (pw4x,pw4y,di_south);
					break;
				case 726: // Pushwall4 East
					PushWall (pw4x,pw4y,di_east);
					break;
				case 727: // Pushwall4 West
					PushWall (pw4x,pw4y,di_west);
					break;
			}
		}
   } 
}

/*
=============================================================================

						   PLAYER CONTROL

=============================================================================
*/



/*
===============
=
= SpawnPlayer
=
===============
*/

void SpawnPlayer (int tilex, int tiley, int dir)
{
	player->obclass = playerobj;
	player->active = true;
	player->tilex = tilex;
	player->tiley = tiley;
	player->areanumber =
		*(mapsegs[0] + farmapylookup[player->tiley]+player->tilex);
	player->x = ((long)tilex<<TILESHIFT)+TILEGLOBAL/2;
	player->y = ((long)tiley<<TILESHIFT)+TILEGLOBAL/2;
	player->state = &s_player;
	player->angle = (1-dir)*90;
	if (player->angle<0)
		player->angle += ANGLES;
	player->flags = FL_NEVERMARK;
	Thrust (0,0);				// set some variables

	InitAreas ();
}


//===========================================================================

/*
===============
=
= T_KnifeAttack
=
= Update player hands, and try to do damage when the proper frame is reached
=
===============
*/

void	KnifeAttack (objtype *ob)
{
	objtype *check,*closest;
	long	dist;

	SD_PlaySound (ATKKNIFESND);
// actually fire
	dist = 0x7fffffff;
	closest = NULL;
	for (check=ob->next ; check ; check=check->next)
		if ( (check->flags & FL_SHOOTABLE)
		&& (check->flags & FL_VISABLE)
		&& abs (check->viewx-centerx) < shootdelta
		)
		{
			if (check->transx < dist)
			{
				dist = check->transx;
				closest = check;
			}
		}

	if (!closest || dist> 0x18000l)
	{
	// missed

		return;
	}

// hit something
	DamageActor (closest,US_RndT() >> 4);
}



void	GunAttack (objtype *ob)
{
	objtype *check,*closest,*oldclosest;
	int		damage;
	int		dx,dy,dist;
	long	viewdist;

	switch (gamestate.weapon)
	{
	case wp_pistol:
		SD_PlaySound (ATKPISTOLSND);
		break;
	case wp_machinegun:
		SD_PlaySound (ATKMACHINEGUNSND);
		break;
	case wp_chaingun:
		SD_PlaySound (ATKGATLINGSND);
		break;
	case wp_rocket:
		SD_PlaySound (MISSILEFIRESND); // recycling sounds because it still works fine
		RocketAttack ();
		return;
	case wp_flamethrower:
		SD_PlaySound (FLAMETHROWERSND); // we only hear this in one level in main wolf3d so it's great to hear it used out of that one
		FlamethrowerAttack ();
		return;
	}

	madenoise = true;

//
// find potential targets
//
	viewdist = 0x7fffffffl;
	closest = NULL;

	while (1)
	{
		oldclosest = closest;

		for (check=ob->next ; check ; check=check->next)
			if ( (check->flags & FL_SHOOTABLE)
			&& (check->flags & FL_VISABLE)
			&& abs (check->viewx-centerx) < shootdelta
			)
			{
				if (check->transx < viewdist)
				{
					viewdist = check->transx;
					closest = check;
				}
			}

		if (closest == oldclosest)
			return;						// no more targets, all missed

	//
	// trace a line from player to enemey
	//
		if (CheckLine(closest))
			break;

	}

//
// hit something
//
	dx = abs(closest->tilex - player->tilex);
	dy = abs(closest->tiley - player->tiley);
	dist = dx>dy ? dx:dy;

	if (dist<2)
		damage = US_RndT() / 4;
	else if (dist<4)
		damage = US_RndT() / 6;
	else
	{
		if ( (US_RndT() / 12) < dist)		// missed
			return;
		damage = US_RndT() / 6;
	}

	DamageActor (closest,damage);
}

/*
===============
=
= RocketAttack
=
= Spawn a rocket from the player's position
===============
*/
// when i was coding this in i had a little bit of a dilemma
// the chaingun was already powerful enough and i found out that the flamethrower and rocket launcher were 5 and 6 sequently
// so i had the decision: nerf the flamethrower
// ...or make it the same and give the rocketlauncher some ungodly damage amount

void RocketAttack (void)
{
	objtype *newobj;
	long	deltax, deltay;

	if (!gamestate.rockets)
		return;

	madenoise = true;

	if (!gamestate.infiniteammo)
		gamestate.rockets--;

	GetNewActor ();
	newobj = new;
	newobj->state = &s_rocket;
	newobj->ticcount = 1;

	newobj->tilex = player->tilex;
	newobj->tiley = player->tiley;
	newobj->x = player->x;
	newobj->y = player->y;
	
	// spawn rocket from player
	deltax = FixedByFrac(0x10000l, costable[player->angle]);
	deltay = -FixedByFrac(0x10000l, sintable[player->angle]);
	newobj->x += deltax;
	newobj->y += deltay;
	
	newobj->obclass = rocketobj;
	newobj->dir = nodir;
	newobj->angle = player->angle % ANGLES;
	newobj->speed = 0x2000l;
	newobj->flags = FL_NONMARK;
	newobj->temp1 = 1;	// playerfired, prob needs a better name but this will do
	newobj->active = true;
}

//===========================================================================

/*
===============
=
= FlamethrowerAttack
=
= Spawn a fireball projectile from flamethrower
===============
*/
// the original wolf3d was coded in such a way that the flames/rockets would phase through enemies
// i figured that we could make this more DOOM like and allow friendly fire between enemies
void FlamethrowerAttack (void)
{
	objtype *newobj;
	long	deltax, deltay;

	if (!gamestate.fuel)
		return;

	madenoise = true;

	if (!gamestate.infiniteammo)
		gamestate.fuel--;

	GetNewActor ();
	newobj = new;
	newobj->state = &s_fire1;
	newobj->ticcount = 1;

	newobj->tilex = player->tilex;
	newobj->tiley = player->tiley;
	newobj->x = player->x;
	newobj->y = player->y;
	
	// you are now fake hitler
	deltax = FixedByFrac(0x10000l, costable[player->angle]);
	deltay = -FixedByFrac(0x10000l, sintable[player->angle]);
	newobj->x += deltax;
	newobj->y += deltay;

	newobj->obclass = fireobj;
	newobj->dir = nodir;
	newobj->angle = player->angle % ANGLES;
	newobj->speed = 0x2000l;
	newobj->flags = FL_NONMARK;
	newobj->temp1 = 1;	// playerfired
	newobj->active = true;
}

//===========================================================================

/*
===============
=
= VictorySpin
=
===============
*/

void VictorySpin (void)
{
	long	desty;

	if (player->angle > 270)
	{
		player->angle -= tics * 3;
		if (player->angle < 270)
			player->angle = 270;
	}
	else if (player->angle < 270)
	{
		player->angle += tics * 3;
		if (player->angle > 270)
			player->angle = 270;
	}

	desty = (((long)player->tiley-5)<<TILESHIFT)-0x3000;

	if (player->y > desty)
	{
		player->y -= tics*4096;
		if (player->y < desty)
			player->y = desty;
	}
}


//===========================================================================

/*
===============
=
= T_Attack
=
===============
*/

void	T_Attack (objtype *ob)
{
	struct	atkinf	*cur;

	UpdateFace ();

	if (gamestate.victoryflag)		// watching the BJ actor
	{
		VictorySpin ();
		return;
	}

	if ( buttonstate[bt_use] && !buttonheld[bt_use] )
		buttonstate[bt_use] = false;

	if ( buttonstate[bt_attack] && !buttonheld[bt_attack])
		buttonstate[bt_attack] = false;

	ControlMovement (ob);
	if (gamestate.victoryflag)		// watching the BJ actor
		return;

	plux = player->x >> UNSIGNEDSHIFT;			// scale to fit in unsigned
	pluy = player->y >> UNSIGNEDSHIFT;
	player->tilex = player->x >> TILESHIFT;		// scale to tile values
	player->tiley = player->y >> TILESHIFT;

//
// change frame and fire
//
	gamestate.attackcount -= tics;
	while (gamestate.attackcount <= 0)
	{
		cur = &attackinfo[gamestate.weapon][gamestate.attackframe];
		switch (cur->attack)
		{
		case -1:
			ob->state = &s_player;
			if (!gamestate.ammo && gamestate.weapon != wp_rocket && gamestate.weapon != wp_flamethrower)
			{
				// if no ammo, rocketlauncher, or flamethrower, switch back to the stabby thing
				gamestate.weapon = wp_knife;
				DrawWeapon ();
			}
			else if (!gamestate.rockets && gamestate.weapon == wp_rocket)
			{
				// if no rockets and weapon is rocket launcher, go to knife
				gamestate.weapon = wp_knife;
				DrawWeapon ();
			}
			else if (!gamestate.fuel && gamestate.weapon == wp_flamethrower)
			{
				//you might get the gist of this already
				gamestate.weapon = wp_knife;
				DrawWeapon ();
			}
			else
			{
				if (gamestate.weapon != gamestate.chosenweapon)
				{
					// once the game bombed out with a divide error because i forgot to include the last fire frame
					if ((gamestate.chosenweapon == wp_rocket && !gamestate.rockets) ||
						(gamestate.chosenweapon == wp_flamethrower && !gamestate.fuel))
					{
						// this was made to prevent a phantom weapon
						// in turn causing The Phantom Pain (is this trademarked by konami)
					}
					else
					{
						gamestate.weapon = gamestate.chosenweapon;
						DrawWeapon ();
					}
				}
			};
			gamestate.attackframe = gamestate.weaponframe = 0;
			return;

		case 4:
			if (gamestate.weapon == wp_rocket)
			{
				if (!gamestate.rockets)
					break;
			}
			else if (gamestate.weapon == wp_flamethrower)
			{
				if (!gamestate.fuel)
					break;
			}
			else
			{
				if (!gamestate.ammo)
					break;
			}
			if (buttonstate[bt_attack])
				gamestate.attackframe -= 2;
		case 1:
			if (gamestate.weapon == wp_rocket)
			{
				if (!gamestate.rockets)
				{	// out of rockets
					gamestate.attackframe++;
					break;
				}
			}
			else if (gamestate.weapon == wp_flamethrower)
			{
				if (!gamestate.fuel)
				{	// out of fuel
					gamestate.attackframe++;
					break;
				}
			}
			else
			{
				if (!gamestate.ammo)
				{	// can only happen with chain gun
					gamestate.attackframe++;
					break;
				}
			}
			GunAttack (ob);
			if (gamestate.weapon != wp_rocket && gamestate.weapon != wp_flamethrower)
				if (!gamestate.infiniteammo)
					gamestate.ammo--;
			DrawAmmo ();
			break;

		case 2:
			KnifeAttack (ob);
			break;

		case 3:
			if (gamestate.weapon == wp_rocket)
			{
				if (gamestate.rockets && buttonstate[bt_attack])
					gamestate.attackframe -= 2;
			}
			else
			{
				if (gamestate.ammo && buttonstate[bt_attack])
					gamestate.attackframe -= 2;
			}
			break;
		}

		gamestate.attackcount += cur->tics;
		gamestate.attackframe++;
		gamestate.weaponframe =
			attackinfo[gamestate.weapon][gamestate.attackframe].frame;
	}

}



//===========================================================================

/*
===============
=
= T_Player
=
===============
*/

void	T_Player (objtype *ob)
{
	if (gamestate.victoryflag)		// watching the BJ actor
	{
		VictorySpin ();
		return;
	}

	UpdateFace ();
	CheckWeaponChange ();

	if ( buttonstate[bt_use] )
		Cmd_Use ();

	if ( buttonstate[bt_attack] && !buttonheld[bt_attack])
		Cmd_Fire ();

	ControlMovement (ob);
	if (gamestate.victoryflag)		// watching the BJ actor
		return;


	plux = player->x >> UNSIGNEDSHIFT;			// scale to fit in unsigned
	pluy = player->y >> UNSIGNEDSHIFT;
	player->tilex = player->x >> TILESHIFT;		// scale to tile values
	player->tiley = player->y >> TILESHIFT;
}

//===========================================================================

/*
===============
=
= WarpPlayer
=
===============
*/
void WarpPlayer (int facedir)
{
objtype  *check;
int  x,y;
int  originalangle = player->angle;  // Save original facing direction

buttonstate[bt_attack] = false;
buttonheld[bt_attack] = false;
gamestate.attackframe = 0;
gamestate.weaponframe = 0;
DrawPlayerWeapon();
ThreeDRefresh();
switch (facedir)
  {
  case EAST:
   check = actorat[warpex][warpey];
   if (check > objlist && (check->flags & FL_SHOOTABLE))
    KillActor(check);
   SpawnPlayer(warpex,warpey,facedir);
   player->angle = originalangle;  // Restore original angle
   break;
  case WEST:
   check = actorat[warpwx][warpwy];
   if (check > objlist && (check->flags & FL_SHOOTABLE))
    KillActor(check);
   SpawnPlayer(warpwx,warpwy,facedir);
   player->angle = originalangle;  // Restore original angle
   break;
  case NORTH:
   check = actorat[warpnx][warpny];
   if (check > objlist && (check->flags & FL_SHOOTABLE))
    KillActor(check);
   SpawnPlayer(warpnx,warpny,facedir);
   player->angle = originalangle;  // Restore original angle
   break;
  case SOUTH:
   check = actorat[warpsx][warpsy];
   if (check > objlist && (check->flags & FL_SHOOTABLE))
    KillActor(check);
   SpawnPlayer(warpsx,warpsy,facedir);
   player->angle = originalangle;  // Restore original angle
   break;
  }

SD_StopSound();
SD_PlaySound (MOVINGSND);
if (DigiMode != sds_Off)
  {
  long lasttimecount = TimeCount;
  while(TimeCount < lasttimecount+60) //adjust warp duration here
   SD_Poll();
  }
else
  VW_WaitVBL(1*50);

buttonstate[bt_attack] = false;
buttonheld[bt_attack] = false;
gamestate.attackframe = 0;
gamestate.weaponframe = 0;
DrawPlayerWeapon();
ThreeDRefresh();
}

//===========================================================================

/*
===============
=
= InstWarpPlayer
=
===============
*/
void InstWarpPlayer (int facedir)
{
objtype  *check;
int  x,y;
int  originalangle = player->angle;  // Save original facing direction

buttonstate[bt_attack] = false;
buttonheld[bt_attack] = false;
gamestate.attackframe = 0;
gamestate.weaponframe = 0;
DrawPlayerWeapon();
ThreeDRefresh();
switch (facedir)
  {
  case WARONE:
   check = actorat[truckox][truckoy];
   if (check > objlist && (check->flags & FL_SHOOTABLE))
    KillActor(check);
   SpawnPlayer(truckox,truckoy,facedir);
   player->angle = originalangle;  // Restore original angle
   break;
  case WARTWO:
   check = actorat[trucktx][truckty];
   if (check > objlist && (check->flags & FL_SHOOTABLE))
    KillActor(check);
   SpawnPlayer(trucktx,truckty,facedir);
   player->angle = originalangle;  // Restore original angle
   break;
  case WARTHR:
   check = actorat[truckhx][truckhy];
   if (check > objlist && (check->flags & FL_SHOOTABLE))
    KillActor(check);
   SpawnPlayer(truckhx,truckhy,facedir);
   player->angle = originalangle;  // Restore original angle
   break;
  case WARFOU:
   check = actorat[truckfx][truckfy];
   if (check > objlist && (check->flags & FL_SHOOTABLE))
	KillActor(check);
   SpawnPlayer(truckfx,truckfy,facedir);
   player->angle = originalangle;  // Restore original angle
   break;
  }

if (DigiMode != sds_Off)
  {
  long lasttimecount = TimeCount;
  while(TimeCount < lasttimecount+1) //adjust warp duration here
   SD_Poll();
  }
else
  VW_WaitVBL(1*50);

buttonstate[bt_attack] = false;
buttonheld[bt_attack] = false;
gamestate.attackframe = 0;
gamestate.weaponframe = 0;
DrawPlayerWeapon();
ThreeDRefresh();
}
//===========================================================================

/*
===============
=
= SpearBaseWarp
=
===============
*/
void SpearBaseWarp (void)
{
	SD_StopSound();
	SD_PlaySound(MOVINGSND);
	if (DigiMode != sds_Off)
	{
		long lasttimecount = TimeCount;
		while(TimeCount < lasttimecount+150)
			SD_Poll();
	}
	else
		SD_WaitSoundDone();

	ClearMemory ();
	gamestate.oldscore = gamestate.score;
	gamestate.mapon = 1;
	DrawLevel ();
	SetupGameLevel ();
	StartMusic ();
	PM_CheckMainMem ();
	player->x = spearx;
	player->y = speary;
	player->angle = spearangle;
}