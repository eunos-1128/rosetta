#!/usr/bin/python

# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

## @file   /GUIs/pyrosetta_toolkit/window_main/input.py
## @brief  Handles the Regional input of the main GUI window
## @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)

#Rosetta Imports
from rosetta import *

#Python Imports
import os

#Tkinter Imports
from Tkinter import *
import tkFileDialog
import tkMessageBox
import tkSimpleDialog

#Toolkit Imports
from modules.Region import Region
from modules.tools import sequence as sequence_tools
from modules.tools import input as input_tools



class InputFrame(Frame):
    
    def __init__(self, main, toolkit, input_class, **options):
        Frame.__init__(self, main, **options)
        self.toolkit = toolkit
        self.input_class = input_class

        self.pwd= self.location()[0]
        
        self.create_GUI_objects()
        self.grid_GUI_objects()

        
    def create_GUI_objects(self):
        
        ############## LOOPS ##############
        #self.label_Loop = Label(self, text="Region Selection", font=("Arial"))
        self.AddLoopButton = Button(self, text = "Add Region", command=lambda: self.addLoop())
        self.RmLoopButton = Button(self, text = "Remove Region", command = lambda:self.remLoop())
        self.StartLoopLabel=Label(self, text="Start of Region:")
        self.EndLoopLabel=Label(self, text="End of Region:")
        self.ChainIDLabel=Label(self, text="Chain ID:")
        self.loops_listbox = Listbox(self)
        self.loops_scroll = Scrollbar(self)
        self.loops_listbox.config(yscrollcommand = self.loops_scroll.set); self.loops_scroll.config(command = self.loops_listbox.yview)
        self.StartLoopEntry=Entry(self, textvariable=self.input_class.loop_start)
        self.EndLoopEntry=Entry(self, textvariable=self.input_class.loop_end)
        self.ChainIDEntry=Entry(self, textvariable=self.input_class.loop_chain)
        ##################################
        
        
        ############ SEQUENCE ############
        
        self.ShoSeqButton = Button(self, text="Show Sequence", command=lambda: self.input_class.loop_sequence.set(sequence_tools.get_sequence(self.toolkit.pose, self.input_class.loop_start.get()+":"+self.input_class.loop_end.get()+":"+self.input_class.loop_chain.get().upper())))
        self.entry_LoopSeq = Entry(self, textvariable=self.input_class.loop_sequence, justify=CENTER)
        
        ##################################
        
        
           ### Photo ###
      
        DesignPhoto =PhotoImage(file = (self.pwd+ "/media/RosettaLogo.gif"))
        self.Photo = Label(master=self, image=DesignPhoto)
        self.Photo.image = DesignPhoto
        

            
    def grid_GUI_objects(self):

        ############# LOOPS #############
        #self.label_Loop.grid(row=11, column=0, columnspan=2, pady=15)
        
        self.loops_listbox.bind("<Double-Button-1>", lambda event: self.remLoop())
        self.StartLoopLabel.grid(row=16, column=0); self.StartLoopEntry.grid(row=16, column=2)
        self.EndLoopLabel.grid(row=17, column=0); self.EndLoopEntry.grid(row=17, column=2)
        self.ChainIDLabel.grid(row=18, column=0); self.ChainIDEntry.grid(row=18, column=2)
        self.AddLoopButton.grid(row=20, column=2, sticky = W+E)
        self.RmLoopButton.grid(row=21, column=2, sticky=W+E)
        self.loops_listbox.grid(row=22, column=0, rowspan=6, columnspan = 1, sticky = W+E, padx=3);
        self.loops_scroll.grid(row=22, column=1, rowspan=6, sticky=E+N+S)
        ################################
        
        
        ########### SEQUENCE ###########
        #self.seq_scroll.grid(row=11, column=2, sticky=S+E+W)
        self.ShoSeqButton.grid(row=19, column=2, sticky=E+W)
        #self.entry_LoopSeq.grid(row=12, column=2)
        
        ################################
        
        
        ########### PYMOL ##############

        self.Photo.grid(row=22, column=2, rowspan=6, columnspan=1, sticky=W+E, padx=3)
        


#### LOOPS ####
    def load_loop(self):
        loops_as_strings = self.input_class.load_loop()
        for loop_string in loops_as_strings:
            loop_stringSP = loop_string.split(":")
            self.input_class.loop_start.set(loop_stringSP[0])
            self.input_class.loop_end.set(loop_stringSP[1])
            self.input_class.loop_chain.set(loop_stringSP[2])
            self.addLoop()
            
    def addLoop(self):
        
        #Current
        looFull = self.input_class.loop_start.get()+ ":"+ self.input_class.loop_end.get()+":"+self.input_class.loop_chain.get().upper()
        if not self.toolkit.pose.total_residue()==0:
            seq = sequence_tools.get_sequence(self.toolkit.pose, self.input_class.loop_start.get()+":"+self.input_class.loop_end.get()+":"+self.input_class.loop_chain.get().upper())
            if seq!="I":
                self.input_class.loop_sequence.set(seq)
            else:
                return
        self.input_class.loops_as_strings.append(looFull)
        self.loops_listbox.insert(END, looFull)
        
        start = looFull.split(":")[0]; end = looFull.split(":")[1]
        #Replacement of loops_as_strings
        
        #Chain
        if (start == "" and end==""):
            region = Region(self.input_class.loop_chain.get().upper(), None, None)
        #Nter
        elif start=="":
            region = Region(self.input_class.loop_chain.get().upper(), None, int(end))
        #Cter
        elif end=="":
            region = Region(self.input_class.loop_chain.get().upper(), int(start), None)
        #Loop
        else: 
            region = Region(self.input_class.loop_chain.get().upper(), int(start), int(end))
            
        self.input_class.regions.add_region(region)
        
    def remLoop(self):
        try:
            #Current
            self.input_class.loops_as_strings.remove(self.loops_listbox.get(self.loops_listbox.curselection()))
        
            #Replacement of loops_as_string
            region_string = self.loops_listbox.get(self.loops_listbox.curselection())

            self.input_class.regions.remove_region(region_string)
        
            self.loops_listbox.delete(self.loops_listbox.curselection())
        except TclError:
            print "Please select a region to remove."
            return
        
    def location(self):
        """
        Allows the script to be self-aware of it's path.
        So that it can be imported/ran from anywhere.
        """
            
        p = os.path.abspath(__file__)
        pathSP = os.path.split(p)
        return pathSP