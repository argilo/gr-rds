/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gr_rds_data_encoder.h>
#include <gr_io_signature.h>
#include <math.h>

gr_rds_data_encoder_sptr gr_rds_make_data_encoder (const char *xmlfile) {
	return gr_rds_data_encoder_sptr (new gr_rds_data_encoder (xmlfile));
}

gr_rds_data_encoder::gr_rds_data_encoder (const char *xmlfile)
  : gr_sync_block ("gr_rds_data_encoder",
			gr_make_io_signature (0, 0, 0),
			gr_make_io_signature (1, 8, sizeof(char)))
{
// initializes the library, checks for potential ABI mismatches
	LIBXML_TEST_VERSION
	reset_rds_data();
	read_xml(xmlfile);
	create_groups(0, false);
}

gr_rds_data_encoder::~gr_rds_data_encoder () {
	xmlCleanupParser();		// Cleanup function for the XML library
	xmlMemoryDump();		// this is to debug memory for regression tests
}

void gr_rds_data_encoder::reset_rds_data(){
	int i=0;
	for(i=0; i<4; i++) {group[i]=0; checkword[i]=0;}
	for(i=0; i<13; i++) buffer[i]=0;
	g0_counter=0;

	PI=0;
	TP=false;
	PTY=0;
	TA=false;
	MuSp=false;
	MS=false;
	AH=false;
	compressed=false;
	static_pty=false;
	memset(PS,' ',sizeof(PS));
	PS[8]='\0';
	memset(radiotext,' ',sizeof(radiotext));
	radiotext[64] = '\0';
	radiotext_AB_flag=0;
}


////////////////////  READING XML FILE  //////////////////

/* assinging the values from the xml file to our variables
 * i could do some more checking for invalid inputs,
 * but leaving as is for now */
void gr_rds_data_encoder::assign_from_xml
(const char *field, const char *value, const int length){
	if(!strcmp(field, "PI")){
		if(length!=4) printf("invalid PI string length: %i\n", length);
		else PI=strtol(value, NULL, 16);
	}
	else if(!strcmp(field, "TP")){
		if(!strcmp(value, "true")) TP=true;
		else if(!strcmp(value, "false")) TP=false;
		else printf("unrecognized TP value: %s\n", value);
	}
	else if(!strcmp(field, "PTY")){
		if((length!=1)&&(length!=2))
			printf("invalid TPY string length: %i\n", length);
		else PTY=atol(value);
	}
	else if(!strcmp(field, "TA")){
		if(!strcmp(value, "true")) TA=true;
		else if(!strcmp(value, "false")) TA=false;
		else printf("unrecognized TA value: %s\n", value);
	}
	else if(!strcmp(field, "MuSp")){
		if(!strcmp(value, "true")) MuSp=true;
		else if(!strcmp(value, "false")) MuSp=false;
		else printf("unrecognized MuSp value: %s\n", value);
	}
	else if(!strcmp(field, "AF1")) AF1=atof(value);
	else if(!strcmp(field, "AF2")) AF2=atof(value);
/* need to copy a char arrays here */
	else if(!strcmp(field, "PS")){
		if(length!=8) printf("invalid PS string length: %i\n", length);
		else for(int i=0; i<length; i++)
			PS[i]=value[i];
	}
	else if(!strcmp(field, "RadioText")){
		if(length>64) printf("invalid RadioText string length: %i\n", length);
		else for(int i=0; i<length; i++)
			radiotext[i]=value[i];
	}
	else printf("unrecognized field type: %s\n", field);
}

/* recursively print the xml nodes */
void gr_rds_data_encoder::print_element_names(xmlNode * a_node){
	xmlNode *cur_node = NULL;
	char *node_name='\0', *attribute='\0', *value='\0';
	int length=0;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE){
			node_name=(char*)cur_node->name;
			if(!strcmp(node_name, "rds")) ;		//move on
			else if(!strcmp(node_name, "group")){
				attribute=(char*)xmlGetProp(cur_node, (const xmlChar *)"type");
				printf("group type: %s\n", attribute);
			}
			else if(!strcmp(node_name, "field")){
				attribute=(char*)xmlGetProp(cur_node, (const xmlChar *)"name");
				value=(char*)xmlNodeGetContent(cur_node);
				length=xmlUTF8Strlen(xmlNodeGetContent(cur_node));
				printf("\t%s: %s (length: %i)\n", attribute, value, length);
				assign_from_xml(attribute, value, length);
			}
			else printf("invalid node name: %s\n", node_name);
		}
		print_element_names(cur_node->children);
	}
}

/* open the xml file, confirm that the root element is "rds",
 * then recursively print it and assign values to the variables.
 * for now, this runs once at startup. in the future, i might want
 * to read periodically (say, each 5 sec?) so as to change values
 * in the xml file and see the results in the "air"... */
int gr_rds_data_encoder::read_xml (const char *xmlfile){
	xmlDoc *doc;
	xmlNode *root_element = NULL;

	doc = xmlParseFile(xmlfile);
	if (doc == NULL) {
		fprintf(stderr, "Failed to parse %s\n", xmlfile);
		return 1;
	}
	root_element = xmlDocGetRootElement(doc);
// The root element MUST be "rds"
	if(strcmp((char*)root_element->name, "rds")){
		fprintf(stderr, "invalid XML root element!\n");
		return 1;
	}
	print_element_names(root_element);

	xmlFreeDoc(doc);
	return 0;
}



//////////////////////  CREATE DATA GROUPS  ///////////////////////

/* see Annex B, page 64 of the standard */
unsigned int gr_rds_data_encoder::calc_syndrome(unsigned long message, 
			unsigned char mlen, unsigned long poly, unsigned char plen) {
	unsigned long reg=0;
	unsigned int i;

	for (i=mlen;i>0;i--)  {
		reg=(reg<<1) | ((message>>(i-1)) & 0x01);
		if (reg & (1<<plen)) reg=reg^poly;
	}
	for (i=plen;i>0;i--) {
		reg=reg<<1;
		if (reg & (1<<plen)) reg=reg^poly;
	}
	return (reg & ((1<<plen)-1));
}

/* see page 41 in the standard; this is an implementation of AF method A
 * FIXME need to add code that declares the number of AF to follow... */
unsigned int gr_rds_data_encoder::encode_af(const double af){
	unsigned int af_code=0;
	if((af>=87.6)&&(af<=107.9))
		af_code=nearbyint((af-87.5)*10);
	else if((af>=153)&&(af<=279))
		af_code=nearbyint((af-144)/9);
	else if((af>=531)&&(af<=1602))
		af_code=nearbyint((af-531)/9+16);
	else
		printf("invalid alternate frequency: %f\n", af);
	return(af_code);
}

/* create the 4 datawords, according to group type.
 * then calculate checkwords and put everything in the buffer. */
void gr_rds_data_encoder::create_groups(const int group_type, const bool AB){
	int i=0;
	group[0]=PI;
	group[1]=(((char)group_type)<<12)|(AB<<11)|(TP<<10)|(PTY<<5);
	if(group_type==0){
		group[1]=group[1]|(TA<<4)|(MuSp<<3);
/* d0=1 (stereo), d1-3=0 */
		if(g0_counter==3) group[1]=group[1]|0x5;
		group[1]=group[1]|(g0_counter&0x3);
		if(!AB)
			group[2]=((encode_af(AF1)&0xff)<<8)|(encode_af(AF2)&0xff);
		else
			group[2]=PI;
		group[3]=(PS[2*g0_counter]<<8)|PS[2*g0_counter+1];
	}
	printf("data: %04X %04X %04X %04X\n", group[0],group[1],group[2],group[3]);

	for(i=0;i<4;i++){
		checkword[i]=calc_syndrome(group[i],16,0x5b9,10);
		group[i]=((group[i]&0xffff)<<10)|(checkword[i]&0x3ff);
	}
	printf("group: %04X %04X %04X %04X\n", group[0],group[1],group[2],group[3]);

/* FIXME there's got to be a better way of doing this */
	buffer[0]=group[0]&0xff;
	buffer[1]=(group[0]>>8)&0xff;
	buffer[2]=(group[0]>>16)&0xff;
	buffer[3]=(group[0]>>24)&0x03;
	buffer[3]=buffer[3]|((group[1]&0x3f)<<2);
	buffer[4]=(group[1]>>6)&0xff;
	buffer[5]=(group[1]>>14)&0xff;
	buffer[6]=(group[1]>>22)&0x0f;
	buffer[6]=buffer[6]|((group[2]&0x0f)<<4);
	buffer[7]=(group[2]>>4)&0xff;
	buffer[8]=(group[2]>>12)&0xff;
	buffer[9]=(group[2]>>20)&0x3f;
	buffer[9]=buffer[9]|((group[3]&0x03)<<6);
	buffer[10]=(group[3]>>2)&0xff;
	buffer[11]=(group[3]>>10)&0xff;
	buffer[12]=(group[3]>>18)&0xff;

	printf("buffer: ");
	for(i=0;i<13;i++) printf("%X ", buffer[i]&0xff);
	printf("\n");
	if(++g0_counter>3) g0_counter=0;
}

/* the plan for now is to do group0 (basic), group2 (radiotext),
 * group4a (clocktime), and group8a (tmc)... */
int gr_rds_data_encoder::work (int noutput_items,
					gr_vector_const_void_star &input_items,
					gr_vector_void_star &output_items)
{
	char *out = (char *) output_items[0];
	int a=0, b=0;
	static int d_buffer_bit_counter=0;

/* FIXME make this output >1 groups */
	for (int i = 0; i < noutput_items; i++){
		d_buffer_bit_counter++;
		if(d_buffer_bit_counter>103) d_buffer_bit_counter=0;
		a=floor(d_buffer_bit_counter/8);
		b=d_buffer_bit_counter%8;
		out[i]=(buffer[a]>>b)&0x1;
//		printf("out[%i]=%i ", d_buffer_bit_counter, (int)out[i]);
	}

	return noutput_items;
}
