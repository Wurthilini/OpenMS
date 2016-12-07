// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2016.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Oliver Alka $
// $Authors: Oliver Alka, Timo Sachsenberg $
// --------------------------------------------------------------------------

//not sure if more #include directives are needed

#include <OpenMS/APPLICATIONS/TOPPBase.h>
#include <OpenMS/DATASTRUCTURES/String.h>
#include <OpenMS/SYSTEM/File.h>
#include <OpenMS/SYSTEM/JavaInfo.h>
#include <OpenMS/DATASTRUCTURES/ListUtils.h>
#include <OpenMS/DATASTRUCTURES/ListUtilsIO.h>
#include <OpenMS/FORMAT/MzMLFile.h>
#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/FORMAT/MzTab.h>
#include <OpenMS/FORMAT/MzTabFile.h>
#include <OpenMS/FORMAT/CsvFile.h>
#include <OpenMS/FORMAT/FileTypes.h>
#include <OpenMS/FORMAT/HANDLERS/MzMLHandler.h>

#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QDir>


using namespace OpenMS;
using namespace std;


//-------------------------------------------------------------
//Doxygen docu
//-------------------------------------------------------------

/**
    @page UTILS_CSIFingerID

    @brief Metabolite identification using single and tandem mass spectrometry.

    CSI:FingerID (Compound Structure Identification: FingerID) is a method for searching a tandem mass spectrum of a small molecule (metabolite) in a database of molecular structures.

    To use this feature, the Sirius command line tool as well as a java installation is needed.

    Sirius can be found on https://bio.informatik.uni-jena.de/software/sirius/

    If you want to use the software with the Gurobi solver (free academic license) instead of GLPK, please follow the instructions in the sirius manual.

    Please see the following publications:

    Kai Dührkop and Sebastian Böcker. Fragmentation trees reloaded.  J Cheminform, 8:5, 2016. (Cite this for fragmentation pattern analysis and fragmentation tree computation)

    Kai Dührkop, Huibin Shen, Marvin Meusel, Juho Rousu, and Sebastian Böcker. Searching molecular structure databases with tandem mass spectra using CSI:FingerID. Proc Natl Acad Sci U S A, 112(41):12580-12585, 2015. (cite this when using CSI:FingerID)

    Sebastian Böcker, Matthias C. Letzel, Zsuzsanna Lipták and Anton Pervukhin. SIRIUS: decomposing isotope patterns for metabolite identification. Bioinformatics (2009) 25 (2): 218-224. (Cite this for isotope pattern analysis)

    Florian Rasche, Aleš Svatoš, Ravi Kumar Maddula, Christoph Böttcher, and Sebastian Böcker. Computing Fragmentation Trees from Tandem Mass Spectrometry Data. Analytical Chemistry (2011) 83 (4): 1243–1251. (Cite this for introduction of fragmentation trees as used by SIRIUS)

    Sebastian Böcker and Florian Rasche. Towards de novo identification of metabolites by analyzing tandem mass spectra. Bioinformatics (2008) 24 (16): i49-i55. (The very first paper to mention fragmentation trees as used by SIRIUS)

    <B>The command line parameters of this tool are:</B>
    @verbinclude UTILS_CSIFingerID.cli
    <B>INI file documentation of this tool:</B>
    @htmlinclude UTILS_CSIFingerID.html
*/

// We do not want this class to show up in the docu:
/// @cond TOPPCLASSES

class TOPPCSIFingerID :
public TOPPBase
{
public:
 TOPPCSIFingerID() :
 TOPPBase("CSIFingerID", "Tool for metabolite identification using single and tandem mass spectrometry", false)
 {
 }

protected:

 struct CSIFingerIDHit
 {
  String inchikey2D;
  String inchi;
  unsigned int rank;
  double score;
  String name;
  String smiles;
  vector<String> pubchemids;
  String links;
 };

 struct CSIFingerIDIdentification
 {
  String id;
  unsigned int scan_index;
  int charge;
  int mz;
  int rt;
  vector<CSIFingerIDHit> hits;
 };

 struct CSIFingerIDRun
 {
  vector<CSIFingerIDIdentification> identifications;
 };

 void registerOptionsAndFlags_()
 {
  registerInputFile_("executable", "<file>", "", "sirius file e.g. /bin/sirius3", true, false, ListUtils::create<String>("skipexists"));
  registerInputFile_("in", "<file>", "", "MzML Input file");
  setValidFormats_("in", ListUtils::create<String>("mzml"));

  registerOutputFile_("out", "<file>", "", " MzTab Output file for CSIFingerID results");
  setValidFormats_("out", ListUtils::create<String>("csv"));

  registerStringOption_("analysis_profile", "<choice>", "qtof", "Specify the used analysis profile", false);
  setValidStrings_("analysis_profile", ListUtils::create<String>("qtof,orbitrap,fticr"));
  registerIntOption_("candidates", "<num>", 5, "The number of candidates in the output. Default 5 best candidates", false);
  registerStringOption_("database", "<choice>", "all", "search formulas in given database", false);
  setValidStrings_("database", ListUtils::create<String>("all,chebi,custom,kegg,bio,natural products,pubmed,hmdb,biocyc,hsdb,knapsack,biological,zinc bio,gnps,pubchem,mesh,maconda"));
  registerIntOption_("noise", "<num>", 0, "median intensity of noise peaks", false);
  registerIntOption_("ppm_max", "<num>", 10, "allowed ppm for decomposing masses", false);
  registerStringOption_("isotope", "<choice>", "both", "how to handle isotope pattern data. Use 'score' to use them for ranking or 'filter' if you just want to remove candidates with bad isotope pattern. With 'both' you can use isotopes for filtering and scoring (default). Use 'omit' to ignore isotope pattern.", false);
  setValidStrings_("isotope", ListUtils::create<String>("score,filter,both,omit"));
  registerStringOption_("elements", "<choice>", "CHNOP[5]S", "The allowed elements. Write CHNOPSCl to allow the elements C, H, N, O, P, S and Cl. Add numbers in brackets to restrict the maximal allowed occurence of these elements: CHNOP[5]S[8]Cl[1]. By default CHNOP[5]S is used.", false);
  registerIntOption_("mass_deviation", "<num>", 5, "Specify the allowed mass deviation of the fragment peak in ppm.", false);
  registerFlag_("iontree", "'--iontree' Print molecular formulas and node labels with the ion formula instead of the neutral formula", false);
  registerFlag_("no_recalibration", "'--no-recalibration' If this option is set, SIRIUS will not recalibrate the spectrum during the analysis.", false);
  registerFlag_("fingerid", "'--fingerid' If this option is set, SIRIUS will search for molecular structure using CSI:FingerId after determining the molecIf this option is set, SIRIUS will search for molecular structure using CSI:FingerId after determining the molecular formulular formula", false);
 }

 ExitCodes main_(int, const char **)
 {
  //-------------------------------------------------------------
  // Parsing parameters
  //-------------------------------------------------------------

  String in = getStringOption_("in");
  String out = getStringOption_("out");

  // Parameter for Sirius3
  QString executable = getStringOption_("executable").toQString();

  QString analysis_profile = getStringOption_("analysis_profile").toQString();
  QString elements = getStringOption_("elements").toQString();
  QString database = getStringOption_("database").toQString();
  QString isotope = getStringOption_("isotope").toQString();
  QString noise = QString::number(getIntOption_("noise"));
  QString ppm_max = QString::number(getIntOption_("ppm_max"));
  QString candidates = QString::number(getIntOption_("candidates"));

  bool no_recalibration = getFlag_("no_recalibration");
  bool fingerid = getFlag_("fingerid");
  bool iontree = getFlag_("iontree");

  //-------------------------------------------------------------
  // Calculations
  //-------------------------------------------------------------

  // laod spectra
  PeakMap spectra;
  MzMLFile f;
  f.setLogType(log_type_);
  f.load(in, spectra);

  CSIFingerIDRun csi_result;

  // loop over all spectra
  for (PeakMap::ConstIterator s_it = spectra.begin(); s_it != spectra.end(); ++s_it)
  {
   // process only MS2 spectra
   if (s_it->getMSLevel() != 2) continue;

   const MSSpectrum<Peak1D>& spectrum = *s_it;

   int scan_index = s_it - spectra.begin();

   const vector<Precursor>& precursor = spectrum.getPrecursors();
   double collision = precursor[0].getActivationEnergy();

   IonSource::Polarity p = spectrum.getInstrumentSettings().getPolarity(); //charge

   // needed later for writing in ms file
   int int_charge(1);
   if (p == IonSource::Polarity::POSITIVE)
   {
    int_charge = +1;
   }
   else
   {
    LOG_WARN << "CSIFingerID only support positive ion mode and mono charged analytes." << endl;
    continue;
   }

   //there should be only one precursor and MS2 should contain peaks to be considered
   if (precursor.size() == 1 && !spectrum.empty())
   {
    //read charge annotated to MS2
    int precursor_charge = precursor[0].getCharge();


    //sirius only supports +1 charge so far
    if (precursor_charge > 1 || precursor_charge <= -1)
    {
     LOG_WARN << "Sirius only support positively mono charged analytes." << endl;
    }

    //get m/z and intensity of precursor != MS1 spectrum
    double precursor_mz = precursor[0].getMZ();
    float precursor_int = precursor[0].getIntensity();
    double spectrum_rt = spectrum.getRT();

    //       if (getPrecursorSpectrum(s_it) != s_it.end())
    //       {
    //        const MSSpectrum<Peak1D>& ms1spectrum = *(getPrecursorSpectrum(s_it));
    //       }

    //store temporary data
    String query_id = String("unknown") + String(scan_index);
    String unique_name =  String(File::getUniqueName()).toQString(); //if not done this way - always new "unique name"
    String tmp_dir = QDir::toNativeSeparators(String(File::getTempDirectory()).toQString()) + "/" + unique_name.toQString() + "_out";
    String tmp_filename = QDir::toNativeSeparators(String(File::getTempDirectory()).toQString()) + "/" + unique_name.toQString() + "_" + query_id.toQString() + ".ms";

    //to get the path and filename
    writeLog_(String("Temp_output_folder: " + tmp_dir));
    writeLog_(String("Temp_filename: " + tmp_filename));
    writeLog_(String("Activation Energy: " + String(collision)));

    // create temporary input file (.ms)
    ofstream os(tmp_filename.c_str());
    if (!os)
    {
     throw Exception::UnableToCreateFile(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, tmp_filename);
    }

    // create temporary output folder
    if (!QDir().mkdir(tmp_dir.toQString()))
    {
     throw Exception::UnableToCreateFile(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, tmp_dir);
    }

    //TODO: MS1 data m/z and intensity of precursor and precursor isotope pattern - for the right sum forumla
    //TODO: Collision energy optional for MS2 - something wrong with .getActivationEnergy() (Precursor)

    //write internal unique .ms data as sirius input
    streamsize prec = os.precision();
    os.precision(6);
    os << fixed;
    os << ">compound " << query_id << "\n"
    << ">parentmass " << precursor_mz << fixed << "\n"
    << ">charge " << int_charge << "\n\n"
    << ">ms1" << "\n"
    << precursor_mz << " " << precursor_int << "\n\n"; //not working properly

    //if collision energy was given - write it into .ms file if not use ms2 instead
    if (collision == 0)
    {
     os << ">ms2" << "\n";
    }
    else
    {
     os << ">collision" << " " << collision << "\n";
    }

    //single spectrum peaks
    for (Size i = 0; i != spectrum.size(); ++i)
    {
     const Peak1D& peak = spectrum[i];
     double mz = peak.getMZ();
     float intensity = peak.getIntensity();
     //intensity has to be higher than zero - Problems wiht QProcess occured if the values with zero intenstiy were given
     if (intensity != 0)
     {
      os << mz << " " << intensity << "\n";
     }
    }
    os.precision(prec); //reset the precision
    os.close();

    QStringList process_params; // the actual process
    process_params << "-p" << analysis_profile
    << "-e" << elements
    << "-d" << database
    << "-s" << isotope
    << "--noise" << noise
    << "-c" << candidates
    << "--ppm-max" << ppm_max
    << "--output" << tmp_dir.toQString(); //internal output folder for temporary files

    if (no_recalibration) process_params << "--no-recalibration";
    if (fingerid) process_params << "--fingerid";
    if (iontree) process_params << "--iontree";

    process_params << tmp_filename.toQString();

    //no terminal output of CSIFingerID/Sirius
    QProcess qp;
    qp.start(executable, process_params); // does automatic escaping etc... start
    bool success = qp.waitForFinished();
    String output(QString(qp.readAllStandardOutput()));

    if (!success || qp.exitStatus() != 0 || qp.exitCode() != 0)
    {
     writeLog_("Fatal error: Running CSIFingerID returned an error code");
     return EXTERNAL_PROGRAM_ERROR;
    }

    //-------------------------------------------------------------
    // writing output
    //-------------------------------------------------------------

    ifstream file(tmp_dir + "/" + unique_name + "_" + query_id +".csv");
    if (file)
    {
     // read results from sirius output files
     CsvFile compounds(tmp_dir + "/" + unique_name + "_" + query_id +".csv", '\t');

     // fill indentification structure containing all candidate hits for a single spectrum
     CSIFingerIDIdentification csi_id;

     for (Size j = 1; j < compounds.rowCount(); ++j)
     {
      StringList sl;
      compounds.getRow(j,sl);
      CSIFingerIDHit csi_hit;
      // parse single candidate hit
      csi_hit.inchikey2D = sl[0];
      csi_hit.inchi = sl[1];
      csi_hit.rank = sl[2].toInt();
      csi_hit.score = sl[3].toDouble();
      csi_hit.name = sl[4];
      csi_hit.smiles = sl[5];
      // split multiple ids: e.g.: 1233423;345345;435345
      sl[6].split(';', csi_hit.pubchemids);
      csi_hit.links = sl[7];
      csi_id.hits.push_back(csi_hit);
     }

     csi_id.scan_index = scan_index;
     csi_id.id = unique_name;
     csi_id.charge = precursor_charge;
     csi_id.mz = precursor_mz;
     csi_id.rt = spectrum_rt;

     csi_result.identifications.push_back(csi_id);

     //clean up temporary input files and output folder
     if (getIntOption_("debug") < 2)
     {
      writeDebug_("Removing temporary files", 1);

      // remove temporary input file
      if (File::exists(tmp_filename) && !File::remove(tmp_filename))
      {
       LOG_WARN << "Unable to remove temporary file: " << tmp_filename << endl;
      }

      // remove temporary output folder
      if (File::exists(tmp_dir) && !File::removeDirRecursively(tmp_dir))
      {
       LOG_WARN << "Unable to remove temporary folder: " << tmp_dir << endl;
      }
     }
     else
     {
      writeDebug_(String("Input to sirius kept for inspection at ") + tmp_filename + "\n", 2);
      writeDebug_(String("Output folder kept for inspection at ") + tmp_dir + "\n", 2);
     }
    }
   }
  } // +}

   // TODO: write out results to mzTab file
   MzTab mztab;
   MzTabFile mztab_out;
   MzTabMetaData md;
   MzTabMSRunMetaData md_run;
   md_run.location = MzTabString(in);
   md.ms_run[1] = md_run;

   MzTabSmallMoleculeSectionRows smsd;
   for (Size i = 0; i != csi_result.identifications.size(); ++i)
   {
    const CSIFingerIDIdentification& id = csi_result.identifications[i];
    for (Size j = 0; j != id.hits.size(); ++j)
    {
     const CSIFingerIDHit& hit = id.hits[j];
     MzTabSmallMoleculeSectionRow smsr;
     smsr.best_search_engine_score[1] = MzTabDouble(hit.score);
     smsr.charge = MzTabDouble(id.charge);
     //smsr.chemical_formula = TODO extract from inchi
     smsr.database = MzTabString(database);
     smsr.description = MzTabString(hit.name);
     smsr.exp_mass_to_charge = MzTabDouble(id.mz);
     vector<MzTabString> pubchemids;
     for (Size k = 0; k != hit.pubchemids.size(); ++k)
     {
      pubchemids.push_back(MzTabString(hit.pubchemids[k]));
     }
     smsr.identifier.set(pubchemids);
     smsr.inchi_key = MzTabString(hit.inchikey2D);
     smsr.smiles = MzTabString(hit.smiles);
     vector<MzTabDouble> rts;
     rts.push_back(MzTabDouble(id.rt));
     smsr.retention_time.set(rts);
     smsr.search_engine.fromCellString("[,,CSIFingerID,3.3]");
     smsr.search_engine_score_ms_run[1] = smsr.best_search_engine_score;
     smsr.spectra_ref.fromCellString(String("ms_run[1]:scan_index=" + String(id.scan_index)));
     smsr.uri = MzTabString(hit.links);
     MzTabOptionalColumnEntry rank;
     rank.first = "rank";
     rank.second = MzTabString(hit.rank);
     smsr.opt_.push_back(rank);
     smsd.push_back(smsr);
    }
   }

   mztab.setSmallMoleculeSectionRows(smsd);
   mztab_out.store(out, mztab);

   return EXECUTION_OK;
 } // - }
};

int main(int argc, const char ** argv)
{
 TOPPCSIFingerID tool;
 return tool.main(argc, argv);
}

/// @endcond
