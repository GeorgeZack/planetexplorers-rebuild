#include "pe_training_text.h"
#include <stddef.h>

/* Source: Assets/Resources/DataBase/localData.bytes Scenario.talk_id ->
 * talk_content localization ID -> Translation.English, verified identical in
 * i18n.db Translation.English. See docs/audit/TRAINING_INTERACTION.md for hashes.
 * Original wording (including spelling) is intentionally preserved. */
static const PeTrainingDialogue dialogue[]={
    {300215,3300215,9041,"Welcome to the simulation deck! It's your turn to take the Survival 101 course. As required by the governors, all colonists are required to acquire the basic survival skills in order to cope with the alien planet. However, you can quit this course at any time by using the exit at the end of the hall."},
    {300258,3300258,-9995,"The default control mode is the 3rd person camera mode. In this mode, press the right mouse button and move the mouse to move the camera. Press F5 to toggle to the other control modes."},
    {300246,3300246,9041,"There are two types of building materials, blocks and voxels. There are also three brush types for building: point, cube and incline."},
    {4075,3004075,9041,"The point brush is for building one block at a time. The cube brush is for click, drag, and pull to build a wall. The incline is for building inclines."},
    {4076,3004076,9041,"The number at the right bottom of each material indicates how many blocks or voxels you possess in inventory. If one of the building brushes is selected, a visual box will appear, which indicates that you can build blocks or voxels at that point."},
    {300249,3300249,9041,"Try to build two walls."},
    {300247,3300247,-9995,"Select the building materials and block types by clicking the \"Menu\" icon above the quick bar. You can drag them to the quick bar for convenience. Choose the brush type by clicking the three different brush icons above the quick bar. (Please note that voxels cannot use the incline brush.)"},
    {300248,3300248,-9995,"The \"T\" button can rotate the block and the \"G\" button can rotate the building directional axis. For this demonstration, please select the cube brush. Hold down the left mouse button and drag to build horizontal. Release the left mouse and move the mouse again to build in the perpendicular direction. (There are more directions on the left side of the screen. It can be closed by clicking the exclamation mark.)"},
    {300230,3300230,9041,"Well done! Hopefully you will not get the chance to test out your skills in the field."},
    {4072,3004072,9041,"Next!"},
    {300231,3300231,9041,"Harvesting resources is pivotal to our survival. There are some simulated plants on the deck. Please go gather them!"},
    {4342,3004342,0,"Now that you know the control layout, let me tell you about the interface."},
    {4343,3004343,0,"The bottom portion of the interface is the quick action bar, on this bar, it shows your health, stamina, level of comfort, and level of hunger. Pressing the health bar will let you enter your status interface."},
    {4344,3004344,0,"On the lower right side of the screen is the menu, from the menu, you can reach all the functions under your command."},
    {4345,3004345,0,"The top right side of the screen shows the mini-map, it is important to always know your general location. Left mouse clicking the mini-map will bring up the world map."},
    {4346,3004346,0,"On the lower right hand corner of the mini-map interface, there are two small icons, one if for showing the mission log, the other is for showing your last 20 conversations. Very handy!"},
    {300216,3300216,9041,"Let us start the lesson. We have simulated the gravity of Maria, which is lower than Earth's. In order to make yourself adapt to it faster, try to move to the other side of the deck. I've prepared some materials needed for the lesson there."},
    {300218,3300218,-9995,"Right click to open the storage."},
    {300219,3300219,-9995,"Right click on the items or drag and drop to move them from one inventory to the other."},
    {300220,3300220,9041,"Have you taken all the materials? Bring them to me."},
    {300221,3300221,9041,"Noted. Medicines are necessities if we want to survive there. Luckily for us, Maria can provide all the necessary raw materials for us to synthesize into different medicines."},
    {300222,3300222,9041,"Drag and drop the most commonly used items to the quick bar at the bottom so that it can easily be accessed when its corresponding number is pressed."},
    {300223,3300223,-9995,"Click \"Inventory\" in the Menu on the bottom right corner of the screen or press \"I\" to open the Inventory window."},
    {1558,3001558,-9999,"What to do next?"},
    {4067,3004067,-9999,"I have the items from the storage box."},
    {4068,3004068,-9995,"Click the x button on the top right corner of the UI to close the window."},
    {4069,3004069,-9995,"When a character has a mission for you, a purple or blue exclamation mark will appear above his or her head. The purple marker is for mainline story missions, the blue one is for sidequests."},
    {4070,3004070,-9995,"Right mouse click on a character or press E when the talk icon appears on the character to interact."},
    {300224,3300224,-9995,"Click the \"Items\" tab, and drag \"herbal juice\" to the quick bar."},
    {4071,3004071,9041,"Good job!"},
    {4298,3004298,-9995,"Pressing the corresponding number on the keyboard enables you to consume it."},
    {4074,3004074,9041,"Confirmed."},
    {300225,3300225,9041,"The Lepus Hare is an aggressive animal abundant on Maria. It is almost inevitable that we will need to deal with them. We have a simulated hare in the course to practice combat skills."},
    {300226,3300226,9041,"Equip the knife from your inventory and prepare for combat."},
    {300227,3300227,-9995,"Click \"Inventory\" in the Menu or press \"I\" to open the Inventory window."},
    {300228,3300228,-9995,"Click the \"Equipment\" tab and right click on \"Knife\" to equip."},
    {300234,3300234,-9995,"Left click to take the tool out after equiping; Get close to the tree and hold the left mouse to harvest when the \"axe\" icon appears."},
    {300235,3300235,9041,"Now that you have harvested some basic materials from the previous lessons, we can start the replication course."},
    {300236,3300236,9041,"The replicator is the crown jewel of 22nd century technology, developed by Dr. Zantora Kess of Kess Industries. By using programmable scripts, the replicator allows you to shape similar molecular based objects into something else. Let us try it out. Here is a script for the wooden shovel."},
    {300237,3300237,-9995,"Click \"Inventory\" in the Menu or press \"I\" to open the inventory window."},
    {300238,3300238,-9995,"Switch to \"Items\" tab. You will find an icon of a wooden shovel with a small USB icon on the lower right corner. That is a script. Right click on the script to install it in the replicator."},
    {300239,3300239,9041,"Now you can replicate the wooden shovel in the replicator if you have enough materials."},
    {300240,3300240,-9995,"Open the replicator window through the menu or press \"Y\" to toggle it up."},
    {300241,3300241,-9995,"Select \"Wooden Shovel\" on the left scroll through and click the hammer icon to start the replication process."},
    {300242,3300242,9041,"Our next lesson is mining. Here we have some simulated terrain. Equip the shovel from the inventory and try to dig."},
    {300243,3300243,-9995,"Equip the shovel from the inventory by right clicking."},
    {4073,3004073,9041,"It's handy, right?"},
    {300244,3300244,9041,"Building shelter is also critical when confronting a brave new world, so it is necessary to learn the basics."},
    {300245,3300245,-9995,"Press \"B\" or click the \"B\" icon on the right side of the quick bar to enter the building mode."},
    {300233,3300233,9041,"Nice job! Let us move on to lumbering. Try to cut down the simulated trees here. You can use the safety axe to cut it down."},
    {300229,3300229,-9995,"Left mouse click to arm the weapon, and left mouse click again to attack. Right click to put away the weapon."},
    {4299,3004299,-9995,"Press \"ESC\" to pop up the \"Options\" window, checking on \"Attack where mouse pointer is\" under the \"Control\" tab can switch attack styles."}
};
const PeTrainingDialogue *pe_training_dialogue_entry(int id){
    for(size_t i=0;i<sizeof dialogue/sizeof *dialogue;i++)if(dialogue[i].dialogue_id==id)return &dialogue[i];
    return NULL;
}
const char *pe_training_dialogue_text(int id){
    const PeTrainingDialogue *entry=pe_training_dialogue_entry(id);return entry?entry->english:NULL;
}
