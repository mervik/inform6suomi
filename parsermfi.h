! parsermfi.h
! ===========
! Muokattuja rutiineja parserm:st� (finnish korvaa alkuper�iset n�ill�).
! Seuraavia rutiineja on muokattu:
!
!     * NounDomain 
!     * PrintCommand
!     * CantSee
!
! from Swedish lib (SweRout.h) with bug fix by F.Ramsberg

! ----------------------------------------------------------------------------
!  Refers works out whether the word at number wnum can refer to the object
!  obj, returning true or false.  The standard method is to see if the
!  word is listed under "name" for the object, but this is more complex
!  in languages other than English.
! ----------------------------------------------------------------------------

[ Refers obj wnum   wd k l m;

    
    if (obj == 0) rfalse;

    #Ifdef LanguageRefers;
    k = LanguageRefers(obj,wnum);    
    if (k >= 0) return k;
    #Endif; ! LanguageRefers

    k = wn; wn = wnum; wd = NextWordStopped(); wn = k;

    if (parser_inflection >= 256) {
        k = indirect(parser_inflection, obj, wd);
        if (k >= 0) return k;
        m = -k;
    }
    else
        m = parser_inflection;
    k = obj.&m; l = (obj.#m)/WORDSIZE-1;
    for (m=0 : m<=l : m++)
        if (wd == k-->m) rtrue;
    rfalse;
];




! ==============================================================================================================
! General bug fix, which became necessary with the swedish grammar
! ==============================================================================================================

! ----------------------------------------------------------------------------
!  The CantSee routine returns a good error number for the situation where
!  the last word looked at didn't seem to refer to any object in context.
!
!  The idea is that: if the actor is in a location (but not inside something
!  like, for instance, a tank which is in that location) then an attempt to
!  refer to one of the words listed as meaningful-but-irrelevant there
!  will cause "you don't need to refer to that in this game" rather than
!  "no such thing" or "what's 'it'?".
!  (The advantage of not having looked at "irrelevant" local nouns until now
!  is that it stops them from clogging up the ambiguity-resolving process.
!  Thus game objects always triumph over scenery.)
! ----------------------------------------------------------------------------

[ CantSee  i w e;
    
    saved_oops=oops_from;

    if (scope_token ~= 0) {
        scope_error = scope_token;
        return ASKSCOPE_PE;
    }

    wn--; w = NextWord();
    e = CANTSEE_PE;
! �#� One line changed
!    if (w==pronoun_word)
    if (w==pronoun_word && ~~ TestScope(pronoun_obj))  ! TestScope condition added
    {
        pronoun__word = pronoun_word; pronoun__obj = pronoun_obj;
        e = ITGONE_PE;
    }
    i = actor; while (parent(i) ~= 0) i = parent(i);

    wn--;
    if (i has visited && Refers(i,wn) == 1) e = SCENERY_PE;
    else {
        Descriptors();  ! skip past THE etc
        if (i has visited && Refers(i,wn) == 1) e = SCENERY_PE;
    }
    wn++;
    if (etype > e) return etype;
    return e;
];





! ----------------------------------------------------------------------------
!  NounDomain does the most substantial part of parsing an object name.
!
!  It is given two "domains" - usually a location and then the actor who is
!  looking - and a context (i.e. token type), and returns:
!
!   0    if no match at all could be made,
!   1    if a multiple object was made,
!   k    if object k was the one decided upon,
!   REPARSE_CODE if it asked a question of the player and consequently rewrote
!        the player's input, so that the whole parser should start again
!        on the rewritten input.
!
!   In the case when it returns 1<k<REPARSE_CODE, it also sets the variable
!   length_of_noun to the number of words in the input text matched to the
!   noun.
!   In the case k=1, the multiple objects are added to multiple_object by
!   hand (not by MultiAdd, because we want to allow duplicates).
! ----------------------------------------------------------------------------



[ NounDomain domain1 domain2 context    first_word i j k l
                                        answer_words marker;
    #Ifdef DEBUG;
    if (parser_trace >= 4) {
        print "   [NounDomain called at word ", wn, "^";
        print "   ";
        if (indef_mode) {
            print "seeking indefinite object: ";
            if (indef_type & OTHER_BIT)  print "other ";
            if (indef_type & MY_BIT)     print "my ";
            if (indef_type & THAT_BIT)   print "that ";
            if (indef_type & PLURAL_BIT) print "plural ";
            if (indef_type & LIT_BIT)    print "lit ";
            if (indef_type & UNLIT_BIT)  print "unlit ";
            if (indef_owner ~= 0) print "owner:", (name) indef_owner;
            new_line;
            print "   number wanted: ";
            if (indef_wanted == 100) print "all"; else print indef_wanted;
            new_line;
            print "   most likely GNAs of names: ", indef_cases, "^";
        }
        else print "seeking definite object^";
    }
    #Endif; ! DEBUG

    match_length = 0; number_matched = 0; match_from = wn; placed_in_flag = 0;

    SearchScope(domain1, domain2, context);

    #Ifdef DEBUG;
    if (parser_trace >= 4) print "   [ND made ", number_matched, " matches]^";
    #Endif; ! DEBUG
  

    
    wn = match_from+match_length;

    ! If nothing worked at all, leave with the word marker skipped past the
    ! first unmatched word...

    if (number_matched == 0) { wn++; rfalse; }

    ! Suppose that there really were some words being parsed (i.e., we did
    ! not just infer).  If so, and if there was only one match, it must be
    ! right and we return it...

    if (match_from <= num_words) {
        if (number_matched == 1) {
            i=match_list-->0;
            return i;
        }

        ! ...now suppose that there was more typing to come, i.e. suppose that
        ! the user entered something beyond this noun.  If nothing ought to follow,
        ! then there must be a mistake, (unless what does follow is just a full
        ! stop, and or comma)

        if (wn <= num_words) {
            i = NextWord(); wn--;
            if (i ~=  AND1__WD or AND2__WD or AND3__WD or comma_word
                   or THEN1__WD or THEN2__WD or THEN3__WD
                   or BUT1__WD or BUT2__WD or BUT3__WD) {
                if (lookahead == ENDIT_TOKEN) rfalse;
            }
        }
    }

    ! Now look for a good choice, if there's more than one choice...

    number_of_classes = 0;

    if (number_matched == 1) i = match_list-->0;
    if (number_matched > 1) {
        i = Adjudicate(context);
        if (i == -1) rfalse;
        if (i == 1) rtrue;       !  Adjudicate has made a multiple
                             !  object, and we pass it on
    }

    ! If i is non-zero here, one of two things is happening: either
    ! (a) an inference has been successfully made that object i is
    !     the intended one from the user's specification, or
    ! (b) the user finished typing some time ago, but we've decided
    !     on i because it's the only possible choice.
    ! In either case we have to keep the pattern up to date,
    ! note that an inference has been made and return.
    ! (Except, we don't note which of a pile of identical objects.)

    if (i ~= 0) {
        if (dont_infer) return i;
        if (inferfrom == 0) inferfrom=pcount;
        pattern-->pcount = i;
        return i;
    }

    ! If we get here, there was no obvious choice of object to make.  If in
    ! fact we've already gone past the end of the player's typing (which
    ! means the match list must contain every object in scope, regardless
    ! of its name), then it's foolish to give an enormous list to choose
    ! from - instead we go and ask a more suitable question...

    if (match_from > num_words) jump Incomplete;

    ! Now we print up the question, using the equivalence classes as worked
    ! out by Adjudicate() so as not to repeat ourselves on plural objects...
  
   if (context==CREATURE_TOKEN) L__M(##Miscellany, 45);
   else		                L__M(##Miscellany, 46);

  
    j = number_of_classes; marker = 0;
    for (i=1 : i<=number_of_classes : i++) {
        while (((match_classes-->marker) ~= i) && ((match_classes-->marker) ~= -i)) marker++;
        k = match_list-->marker;


!! Vastaukset 45 ja 46 - "ota ruokaa"... "Tarkoitatko pahaa ruokaa vai hyv�� ruokaa?",
!! tai "ota ruoka"... "Paha ruoka vai hyv� ruoka?"
!! jos sy�te on nominatiivi, ei tulosta "Tarkoitatko ", vaan ensimm�inen sana isolla alkuk.

        if ((match_classes-->marker > 0) && (csLR == 0 or 1)) 
         	{ if (i == 1) print (k_nominatiivi) k; else print (nominatiivi) k; }; 		

!! jos sy�te on muu kuin nominatiivi...

        if ((i == 1) && (csLR ~= 0 or 1)) print "Tarkoitatko ";
  
        if ((match_classes-->marker > 0) && (csLR ~= 0 or 1))
        	switch (csLR) {
        !!-	0: print (nominatiivi) k; 
		2: print (genetiivi) k;
		3: print (partitiivi) k;
		4: print (inessiivi) k;
		5: print (elatiivi) k;
		6: print (illatiivi) k;
		7: print (adessiivi) k;
		8: print (ablatiivi) k;
		9: print (allatiivi) k;
		10: print (essiivi) k;
		11: print (translatiivi) k;
		};

        if (i < j-1)  print (string) COMMA__TX;

         if (i == j-1) print (string) OR__TX; !!!# OR__TX on "vai"
        
    } 
    L__M(##Miscellany, 57); !!!# kysymysmerkki ja rivinvaihto
    ! ...and get an answer:

  .WhichOne;
    #Ifdef TARGET_ZCODE;
    for (i=2 : i<INPUT_BUFFER_LEN : i++) buffer2->i = ' ';
    #Endif; ! TARGET_ZCODE
    
    answer_words=Keyboard(buffer2, parse2);
    ! Conveniently, parse2-->1 is the first word in both ZCODE and GLULX.
    first_word = (parse2-->1);

    ! Take care of "all", because that does something too clever here to do
    ! later on:

    if (first_word == ALL1__WD or ALL2__WD or ALL3__WD or ALL4__WD or ALL5__WD) {
        if (context == MULTI_TOKEN or MULTIHELD_TOKEN or MULTIEXCEPT_TOKEN or MULTIINSIDE_TOKEN) {
            l = multiple_object-->0;
            for (i=0 : i<number_matched && l+i<63 : i++) {
                k = match_list-->i;
                multiple_object-->(i+1+l) = k;
            }
            multiple_object-->0 = i+l;
            rtrue;
        }
        L__M(##Miscellany, 47);
        jump WhichOne;
    }

    ! If the first word of the reply can be interpreted as a verb, then
    ! assume that the player has ignored the question and given a new
    ! command altogether.
    ! (This is one time when it's convenient that the directions are
    ! not themselves verbs - thus, "north" as a reply to "Which, the north
    ! or south door" is not treated as a fresh command but as an answer.)

    #Ifdef LanguageIsVerb;
    if (first_word == 0) {
        j = wn; first_word = LanguageIsVerb(buffer2, parse2, 1); wn = j;
    }
    #Endif; ! LanguageIsVerb
    if (first_word ~= 0) {
        j = first_word->#dict_par1;
        if ((0 ~= j&1) && ~~LanguageVerbMayBeName(first_word)) {
            CopyBuffer(buffer, buffer2);
            return REPARSE_CODE;
        }
    }

    ! Now we insert the answer into the original typed command, as
    ! words additionally describing the same object
    ! (eg, > take red button
    !      Which one, ...
    !      > music
    ! becomes "take music red button".  The parser will thus have three
    ! words to work from next time, not two.)

    #Ifdef TARGET_ZCODE;
    k = WordAddress(match_from) - buffer; l=buffer2->1+1;
     for (j=buffer + buffer->0 - 1 : j>=buffer+k+l : j--) j->0 = 0->(j-l);
     for (i=0 : i<l : i++) buffer->(k+i) = buffer2->(2+i);

    buffer->(k+l-1) = ' '; 
    buffer->1 = buffer->1 + l;
    if (buffer->1 >= (buffer->0 - 1)) buffer->1 = buffer->0;
    #Ifnot; ! TARGET_GLULX
    k = WordAddress(match_from) - buffer;
    l = (buffer2-->0) + 1;
    for (j=buffer+INPUT_BUFFER_LEN-1 : j>=buffer+k+l : j--) j->0 = j->(-l);
    for (i=0 : i<l : i++) buffer->(k+i) = buffer2->(WORDSIZE+i);
    buffer->(k+l-1) = ' ';
    buffer-->0 = buffer-->0 + l;
    if (buffer-->0 > (INPUT_BUFFER_LEN-WORDSIZE)) buffer-->0 = (INPUT_BUFFER_LEN-WORDSIZE);
    #Endif; ! TARGET_

    ! Having reconstructed the input, we warn the parser accordingly
    ! and get out.

    return REPARSE_CODE;

    ! Now we come to the question asked when the input has run out
    ! and can't easily be guessed (eg, the player typed "take" and there
    ! were plenty of things which might have been meant).

  .Incomplete;

    if (context == CREATURE_TOKEN) L__M(##Miscellany, 48);
    else                           L__M(##Miscellany, 49);

    #Ifdef TARGET_ZCODE;
    for (i=2 : i<INPUT_BUFFER_LEN : i++) buffer2->i=' ';
    #Endif; ! TARGET_ZCODE
    answer_words = Keyboard(buffer2, parse2);

    first_word=(parse2-->1);
    #Ifdef LanguageIsVerb;
    if (first_word==0) {
        j = wn; first_word=LanguageIsVerb(buffer2, parse2, 1); wn = j;
    }
    #Endif; ! LanguageIsVerb

    ! Once again, if the reply looks like a command, give it to the
    ! parser to get on with and forget about the question...

    if (first_word ~= 0) {
        j = first_word->#dict_par1;
        if (0 ~= j&1) {
            CopyBuffer(buffer, buffer2);
            return REPARSE_CODE;
        }
    }

    ! ...but if we have a genuine answer, then:
    !
    ! (1) we must glue in text suitable for anything that's been inferred.

    if (inferfrom ~= 0) {
        for (j=inferfrom : j<pcount : j++) {
            if (pattern-->j == PATTERN_NULL) continue;
            #Ifdef TARGET_ZCODE;
            i = 2+buffer->1; (buffer->1)++; buffer->(i++) = ' ';
            #Ifnot; ! TARGET_GLULX
            i = WORDSIZE + buffer-->0;
            (buffer-->0)++; buffer->(i++) = ' ';
            #Endif; ! TARGET_

            #Ifdef DEBUG;
            if (parser_trace >= 5) print "[Gluing in inference with pattern code ", pattern-->j, "]^";
            #Endif; ! DEBUG

            ! Conveniently, parse2-->1 is the first word in both ZCODE and GLULX.

            parse2-->1 = 0;

            ! An inferred object.  Best we can do is glue in a pronoun.
            ! (This is imperfect, but it's very seldom needed anyway.)

            if (pattern-->j >= 2 && pattern-->j < REPARSE_CODE) {
                PronounNotice(pattern-->j);
                for (k=1 : k<=LanguagePronouns-->0 : k=k+3)
                    if (pattern-->j == LanguagePronouns-->(k+2)) {
                        parse2-->1 = LanguagePronouns-->k;
                        #Ifdef DEBUG;
                        if (parser_trace >= 5) print "[Using pronoun '", (address) parse2-->1, "']^";
                        #Endif; ! DEBUG
                        break;
                    }
            }
            else {
                ! An inferred preposition.
                parse2-->1 = No__Dword(pattern-->j - REPARSE_CODE);
                #Ifdef DEBUG;
                if (parser_trace >= 5) print "[Using preposition '", (address) parse2-->1, "']^";
                #Endif; ! DEBUG
            }

            ! parse2-->1 now holds the dictionary address of the word to glue in.

            if (parse2-->1 ~= 0) {
                k = buffer + i;
                #Ifdef TARGET_ZCODE;
                @output_stream 3 k;
                 print (address) parse2-->1;
                @output_stream -3;
                k = k-->0;
                for (l=i : l<i+k : l++) buffer->l = buffer->(l+2);
                i = i + k; buffer->1 = i-2;
                #Ifnot; ! TARGET_GLULX
                k = PrintAnyToArray(buffer+i, INPUT_BUFFER_LEN-i, parse2-->1);
                i = i + k; buffer-->0 = i - WORDSIZE;
                #Endif; ! TARGET_
            }
        }
    }

    ! (2) we must glue the newly-typed text onto the end.

    #Ifdef TARGET_ZCODE;
    i = 2+buffer->1; (buffer->1)++; buffer->(i++) = ' ';
    for (j=0 : j<buffer2->1 : i++,j++) {
        buffer->i = buffer2->(j+2);
        (buffer->1)++;
        if (buffer->1 == INPUT_BUFFER_LEN) break;
    }
    #Ifnot; ! TARGET_GLULX
    i = WORDSIZE + buffer-->0;
    (buffer-->0)++; buffer->(i++) = ' ';
    for (j=0 : j<buffer2-->0 : i++,j++) {
        buffer->i = buffer2->(j+WORDSIZE);
        (buffer-->0)++;
        if (buffer-->0 == INPUT_BUFFER_LEN) break;
    }
    #Endif; ! TARGET_

    ! (3) we fill up the buffer with spaces, which is unnecessary, but may
    !     help incorrectly-written interpreters to cope.

    #Ifdef TARGET_ZCODE;
    for (: i<INPUT_BUFFER_LEN : i++) buffer->i = ' ';
    #Endif; ! TARGET_ZCODE

    return REPARSE_CODE;

]; ! end of NounDomain

!!!# PrintCommand 

! ----------------------------------------------------------------------------
!  PrintCommand reconstructs the command as it presently reads, from
!  the pattern which has been built up
!
!  If from is 0, it starts with the verb: then it goes through the pattern.
!  The other parameter is "emptyf" - a flag: if 0, it goes up to pcount:
!  if 1, it goes up to pcount-1.
!
!  Note that verbs and prepositions are printed out of the dictionary:
!  and that since the dictionary may only preserve the first six characters
!  of a word (in a V3 game), we have to hand-code the longer words needed.
!
!  (Recall that pattern entries are 0 for "multiple object", 1 for "special
!  word", 2 to REPARSE_CODE-1 are object numbers and REPARSE_CODE+n means the
!  preposition n)
! ----------------------------------------------------------------------------

[ PrintCommand from i k spacing_flag; 
    
    if (from == 0) {
       	i = verb_word;  
        if (LanguageVerb(i) == 0)
            if (PrintVerb(i) == 0) print (address) i;
        from++; spacing_flag = true;
	
    }

    for (k=from : k<pcount : k++) { 	
        i = pattern-->k;
        if (i == PATTERN_NULL) continue;
        if (spacing_flag) print (char) ' ';
        if (i ==0 ) { print (string) THOSET__TX; jump TokenPrinted; }
        if (i == 1) { print (string) THAT__TX;   jump TokenPrinted; }
        if (i >= REPARSE_CODE)     
                    print (address) No__Dword(i-REPARSE_CODE);
        else
	
            if (i in compass && LanguageVerbLikesAdverb(verb_word))
                LanguageDirection (i.door_dir); ! the direction name
	! as adverb
            else
		!!! muokattu
		! (CaseIs ~= 0 && k >= 2) ! jos t�m�, 1. sana nom. 2. sana taipuu			
             	if (CaseIs ~= 0) 
             		switch (CaseIs) {
                 	csNom: print (nominatiivi) i;
             		csGen: print (genetiivi) i;
             		csPar: print (partitiivi) i; 
			csIne: print (inessiivi) i;
			csIll: print (illatiivi) i;
			csAde: print (adessiivi) i; 
			csAbl: print (ablatiivi) i;
			csAll: print (allatiivi) i; 
			csEss: print (essiivi) i;
			csTra: print (translatiivi) i;}
                       
 		else print (the) i;
	
    	
    .TokenPrinted;
	spacing_flag = true;

	
    }

    ! esim. "laita kuutio mihin?"
    PrintKysymys(verb_word, from, k); 	
    !!! muokattu loppu

];
