<<<<USE COURIER REGULAR 10 FONT IF YOU WOULD LIKE TO PRINT THIS DOCUMENT>>>>>

Debug Release Readme for <IWSVA 6.5 SP2 Patch2>
------------------------------------------------------------------------------------
NOTICE: This debug release was developed to collect additional logs or 
information for resolving a customer-reported problem.  As such, this debug 
release is not certified as an official product update and should be replaced 
by a hot fix or official product update. Consequently, THIS DEBUG RELEASE IS 
PROVIDED "AS-IS".  TREND MICRO MAKES NO WARRANTY OR PROMISE ABOUT THE OPERATION 
OR PERFORMANCE OF THIS DEBUG RELEASE NOR DOES IT WARRANT THAT THIS DEBUG RELEASE 
IS ERROR-FREE.  TO THE FULLEST EXTENT PERMITTED BY LAW, TREND MICRO DISCLAIMS 
ALL IMPLIED AND STATUTORY WARRANTIES, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.

------------------------------------------------------------------------------------

1. Symptoms
   This dubug moudle is to expand to 256 custom categories and it is based on IWSVA 6.5 SP2
   Patch2 1775, please make sure the current version is (or larger than) 1775 before apply this debug module.

2. Information planning to collect by this debug release
   
3. Procedures for applying the debug release

3.1 unpack the debug_module
    $tar zxvf debug-build-customcategory.tgz
    
3.2 apply the debug module:
    $ cd debug-build-customcategory
    $ ./apply.sh install


3.3 rollback
    $./apply.sh rollback


5.  Contact Information  

   A license to the Trend Micro software usually includes the right to 
   product updates,pattern file updates, and basic technical support for 
   one (1) year from the date of purchase only.  After the first year, 
   Maintenance must be renewed on an annual basis at Trend Micro's 
   then-current Maintenance fees.
   
   You can contact Trend Micro via fax, phone, and email, or visit us at:
 
   FTP://www.trendmicro.com 

   Evaluation copies of Trend Micro products can be downloaded from our 
   Web site.
 
   Global Mailing Address/Telephone Numbers
   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   For global contact information in the Asia/Pacific region, Australia
   and New Zealand, Europe, Latin America, and Canada, refer to:

   FTP://www.trendmicro.com/en/about/overview.htm

   The Trend Micro "About Us" screen displays. Click the appropriate 
   link in the "Contact Us" section of the screen.

   Note: This information is subject to change without notice.



