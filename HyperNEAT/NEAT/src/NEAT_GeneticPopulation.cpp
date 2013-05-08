#include "NEAT_GeneticPopulation.h"

#include "NEAT_GeneticGeneration.h"



#include "NEAT_GeneticIndividual.h"
#include "NEAT_Random.h"
#include <algorithm>

#include <boost/algorithm/string.hpp>

namespace NEAT
{

    GeneticPopulation::GeneticPopulation()
            : onGeneration(0)
    {

        {
            generations.push_back(
                shared_ptr<GeneticGeneration>(
                    new GeneticGeneration(0)
                )
            );
        }
    }

    GeneticPopulation::GeneticPopulation(
        string fileName
    )
            : onGeneration(-1)
    {
        TiXmlDocument doc(fileName);

        bool loadStatus;

        if (iends_with(fileName,".gz"))
        {
            loadStatus = doc.LoadFileGZ();
        }
        else
        {
            loadStatus = doc.LoadFile();
        }

        if (!loadStatus)
        {
            throw CREATE_LOCATEDEXCEPTION_INFO("Error trying to load the XML file!");
        }

        TiXmlElement *root;

        root = doc.FirstChildElement();

        {
            TiXmlElement *generationElement = root->FirstChildElement("GeneticGeneration");

            while (generationElement)
            {
                if(generationElement->FirstChildElement("Individual") != NULL)
                    generations.push_back(shared_ptr<GeneticGeneration>(new GeneticGeneration(generationElement)));

                generationElement = generationElement->NextSiblingElement("GeneticGeneration");
                onGeneration++;
            }
        }

        if (onGeneration<0)
        {
            throw CREATE_LOCATEDEXCEPTION_INFO("Tried to load a population with no generations!");
        }
        
        //if(Globals::getSingleton()->getParameterValue("MultiObjective", 0.0) < 0.5) {
            adjustFitness();
        //}
    }

    GeneticPopulation::~GeneticPopulation()
    {
        while (!species.empty())
            species.erase(species.begin());

        while (!extinctSpecies.empty())
            extinctSpecies.erase(extinctSpecies.begin());
    }

    int GeneticPopulation::getIndividualCount(int generation)
    {
        if (generation==-1)
            generation=int(onGeneration);

        return generations[generation]->getIndividualCount();
    }

    void GeneticPopulation::addIndividual(shared_ptr<GeneticIndividual> individual)
    {
        generations[onGeneration]->addIndividual(individual);
    }

    shared_ptr<GeneticIndividual> GeneticPopulation::getIndividual(int a,int generation)
    {
        //cout << a << ',' << generation << endl;

        if (generation==-1)
        {
            //cout << "NO GEN GIVEN: USING onGeneration\n";
            generation=int(onGeneration);
        }

        if (generation>=int(generations.size())||a>=generations[generation]->getIndividualCount())
        {
            cout << "GET_INDIVIDUAL: GENERATION OUT OF RANGE!\n";
            throw CREATE_LOCATEDEXCEPTION_INFO("GET_INDIVIDUAL: GENERATION OUT OF RANGE!\n");
        }

        return generations[generation]->getIndividual(a);
    }

    vector<shared_ptr<GeneticIndividual> >::iterator GeneticPopulation::getIndividualIterator(int a,int generation)
    {
        if (generation==-1)
            generation=int(onGeneration);

        if (generation>=int(generations.size())||a>=generations[generation]->getIndividualCount())
        {
            throw CREATE_LOCATEDEXCEPTION_INFO("ERROR: Generation out of range!\n");
        }

        return generations[generation]->getIndividualIterator(a);
    }

    shared_ptr<GeneticIndividual> GeneticPopulation::getBestAllTimeIndividual()
    {
        shared_ptr<GeneticIndividual> bestIndividual;

        for (int a=0;a<(int)generations.size();a++)
        {
            for (int b=0;b<generations[a]->getIndividualCount();b++)
            {
                shared_ptr<GeneticIndividual> individual = generations[a]->getIndividual(b);
                if (bestIndividual==NULL||bestIndividual->getFitness()<=individual->getFitness())
                    bestIndividual = individual;
            }
        }

        return bestIndividual;
    }

    shared_ptr<GeneticIndividual> GeneticPopulation::getBestIndividualOfGeneration(int generation)
    {
        shared_ptr<GeneticIndividual> bestIndividual;

        if (generation==-1)
            generation = int(generations.size())-1;

        for (int b=0;b<generations[generation]->getIndividualCount();b++)
        {
            shared_ptr<GeneticIndividual> individual = generations[generation]->getIndividual(b);
            if (bestIndividual==NULL||bestIndividual->getFitness()<individual->getFitness())
                bestIndividual = individual;
        }

        return bestIndividual;
    }

    void GeneticPopulation::speciate()
    {
        double compatThreshold = Globals::getSingleton()->getParameterValue("CompatibilityThreshold");

        for (int a=0;a<generations[onGeneration]->getIndividualCount();a++)
        {
            shared_ptr<GeneticIndividual> individual = generations[onGeneration]->getIndividual(a);

            bool makeNewSpecies=true;

            for (int b=0;b<(int)species.size();b++)
            {
                double compatibility = species[b]->getBestIndividual()->getCompatibility(individual);
                if (compatibility<compatThreshold)
                {
                    //Found a compatible species
                    individual->setSpeciesID(species[b]->getID());
                    makeNewSpecies=false;
                    break;
                }
            }

            if (makeNewSpecies)
            {
                //Make a new species.  The process of making a new speceis sets the ID for the individual.
                shared_ptr<GeneticSpecies> newSpecies(new GeneticSpecies(individual));
                species.push_back(newSpecies);
            }
        }

        int speciesTarget = int(Globals::getSingleton()->getParameterValue("SpeciesSizeTarget"));

        double compatMod;

        if ((int)species.size()<speciesTarget)
        {
            compatMod = -Globals::getSingleton()->getParameterValue("CompatibilityModifier");
        }
        else if ((int)species.size()>speciesTarget)
        {
            compatMod = +Globals::getSingleton()->getParameterValue("CompatibilityModifier");
        }
        else
        {
            compatMod=0.0;
        }

        if (compatThreshold<(fabs(compatMod)+0.3)&&compatMod<0.0)
        {
            //This is to keep the compatibility threshold from going ridiculusly small.
            if (compatThreshold<0.001)
                compatThreshold = 0.001;

            compatThreshold/=2.0;
        }
        else if (compatThreshold<(compatMod+0.3))
        {
            compatThreshold*=2.0;
        }
        else
        {
            compatThreshold+=compatMod;
        }

        Globals::getSingleton()->setParameterValue("CompatibilityThreshold",compatThreshold);
    }

    void GeneticPopulation::setSpeciesMultipliers()
    {}

    void GeneticPopulation::adjustFitness()
    {
        if(Globals::getSingleton()->getParameterValue("MultiObjective", 0.0) > 0.5) {

            if(Globals::getSingleton()->getParameterValue("GenotypicDiversity", 0.0) > 0.5) {

                for (int p=0;p<generations[onGeneration]->getIndividualCount();p++)
                {
                    shared_ptr<GeneticIndividual> individual1 = generations[onGeneration]->getIndividual(p);
                    vector<double> compatabilities;
                    for (int q=0;q<generations[onGeneration]->getIndividualCount();q++) {
                        if(p!=q) {
                            shared_ptr<GeneticIndividual> individual2 = generations[onGeneration]->getIndividual(q);
                            compatabilities.push_back(individual1->getCompatibility(individual2));
                        }
                    }
                    double compatabilitySum = 0;
                    stable_sort(compatabilities.begin(), compatabilities.end());
                    for(int i = 0; i<int(Globals::getSingleton()->getParameterValue("GenotypicDiversityK")) && i<compatabilities.size(); i++) {
	                        compatabilitySum += compatabilities[i];
                    }

                    vector<double> fitnesses = individual1->getFitnesses();
                    //cout << fitnesses.size() << " ";
		            int numFitnesses = int(Globals::getSingleton()->getParameterValue("NumFitnesses", 2.0));
                    if(fitnesses.size() < (numFitnesses + 1))
                        fitnesses.push_back(compatabilitySum);
                    else
                        fitnesses[numFitnesses] = compatabilitySum;
                    individual1->setFitnesses(fitnesses);
                }
            }

            rankByDominance();
        } else {
	    	bool sortFirst = (Globals::getSingleton()->getParameterValue("SortBeforeSpeciate", 0.0) > 0.5);  //setting this param to true makes algorithm behave as published
            if(sortFirst) {
	            //This function sorts the individuals by fitness
	            generations[onGeneration]->sortByFitness();
            }


            speciate();


            for (int a=0;a<(int)species.size();a++)
            {
                species[a]->resetIndividuals();
            }

            for (int a=0;a<generations[onGeneration]->getIndividualCount();a++)
            {
                shared_ptr<GeneticIndividual> individual = generations[onGeneration]->getIndividual(a);

                getSpecies(individual->getSpeciesID())->addIndividual(individual);
            }

            for (int a=0;a<(int)species.size();a++)
            {
                if (species[a]->getIndividualCount()==0)
                {
                    //extinctSpecies.push_back(species[a]);
                    species.erase(species.begin()+a);
                    a--;
                }
            }

            for (int a=0;a<(int)species.size();a++)
            {
                species[a]->setMultiplier();
                species[a]->setFitness();
                species[a]->incrementAge();
            }


	    if(!sortFirst) {
	            //This function sorts the individuals by fitness
        	    generations[onGeneration]->sortByFitness();
	    }
        }
    }

    void GeneticPopulation::produceNextGeneration()
    {
        if(Globals::getSingleton()->getParameterValue("MultiObjective", 0.0) > 0.5) {
            produceNextGenerationMultiObjective();
            return;
        } else if(Globals::getSingleton()->getParameterValue("ShadowModel", 0.0) > 0.5) {
            produceNextGenerationShadow();
            return;
        }

        cout << "In produce next generation loop...\n";
        //This clears the link history so future links with the same toNode and fromNode will have different IDs
        Globals::getSingleton()->clearLinkHistory();

        int numParents = int(generations[onGeneration]->getIndividualCount());

        for(int a=0;a<numParents;a++)
        {
            if(generations[onGeneration]->getIndividual(a)->getFitness() < 1e-6)
            {
                throw CREATE_LOCATEDEXCEPTION_INFO("ERROR: Fitness must be a positive number!\n");
            }
        }

        double totalFitness=0;

        for (int a=0;a<(int)species.size();a++)
        {
            totalFitness += species[a]->getAdjustedFitness();
        }
        int totalOffspring=0;
        for (int a=0;a<(int)species.size();a++)
        {
            double adjustedFitness = species[a]->getAdjustedFitness();
            int offspring = int(adjustedFitness/totalFitness*numParents);
            totalOffspring+=offspring;
            species[a]->setOffspringCount(offspring);
        }
        //cout << "Pausing\n";
        //system("PAUSE");
        //Some offspring were truncated.  Give these to the best individuals
        while (totalOffspring<numParents)
        {
            for (int a=0;totalOffspring<numParents&&a<generations[onGeneration]->getIndividualCount();a++)
            {
                shared_ptr<GeneticIndividual> ind = generations[onGeneration]->getIndividual(a);
                shared_ptr<GeneticSpecies> gs = getSpecies(ind->getSpeciesID());
                gs->setOffspringCount(gs->getOffspringCount()+1);
                totalOffspring++;

                /*
                //Try to give 2 offspring to the very best individual if it only has one offspring.
                //This fixes the problem where the best indiviudal sits in his own species
                //and duplicates every generation.
                if(a==0&&gs->getOffspringCount()==1&&totalOffspring<numParents)
                {
                gs->setOffspringCount(gs->getOffspringCount()+1);
                totalOffspring++;
                }*/

            }
        }
        for (int a=0;a<(int)species.size();a++)
        {
            cout << "Species ID: " << species[a]->getID() << " Age: " << species[a]->getAge() << " last improv. age: " << species[a]->getAgeOfLastImprovement() << " Fitness: " << species[a]->getFitness() << "*" << species[a]->getMultiplier() << "=" << species[a]->getAdjustedFitness() <<  " Size: " << int(species[a]->getIndividualCount()) << " Offspring: " << int(species[a]->getOffspringCount()) << endl;
        }

        //This is the new generation
        vector<shared_ptr<GeneticIndividual> > babies;

        double totalIndividualFitness=0;

        for (int a=0;a<(int)species.size();a++)
        {
            species[a]->setReproduced(false);
        }

        int smallestSpeciesSizeWithElitism
        = int(Globals::getSingleton()->getParameterValue("SmallestSpeciesSizeWithElitism"));
        double mutateSpeciesChampionProbability
        = Globals::getSingleton()->getParameterValue("MutateSpeciesChampionProbability");
        bool forceCopyGenerationChampion
        = (
              Globals::getSingleton()->getParameterValue("ForceCopyGenerationChampion")>
              Globals::getSingleton()->getRandom().getRandomDouble()
          );

        //cout << "generation fitnesses\n";

        for (int a=0;a<generations[onGeneration]->getIndividualCount();a++)
        {
            //Go through and add the species champions
            shared_ptr<GeneticIndividual> ind = generations[onGeneration]->getIndividual(a);
            //cout << "********* " << ind->getFitness() << "\n";
            shared_ptr<GeneticSpecies> species = getSpecies(ind->getSpeciesID());
            if (!species->isReproduced())
            {
                species->setReproduced(true);
                //This is the first and best organism of this species to be added, so it's the species champion
                //of this generation
                if (ind->getFitness()>species->getBestIndividual()->getFitness())
                {
                    //We have a new all-time species champion!
                    species->setBestIndividual(ind);
                    cout << "Species " << species->getID() << " has a new champ with fitness " << species->getBestIndividual()->getFitness() << endl;
                }

                if ((a==0&&forceCopyGenerationChampion)||(species->getOffspringCount()>=smallestSpeciesSizeWithElitism))
                {
                    //Copy species champion.
                    bool mutateChampion;
                    if (Globals::getSingleton()->getRandom().getRandomDouble()<mutateSpeciesChampionProbability)
                        mutateChampion = true;
                    else
                        mutateChampion = false;


//                   cout << "copying champ..., mutating: "  << mutateChampion << ", fitness: " << ind->getFitness();
                    babies.push_back(shared_ptr<GeneticIndividual>(new GeneticIndividual(ind,mutateChampion)));
                    ind->setHasReproduced(true);
		    /*if(a==0&&forceCopyGenerationChampion) {
		    	babies[babies.size()-1]->setFitness(ind->getFitness());
		    	vector<double> fitnesses;
		    	for(int i=0; i<ind->getFitnesses().size(); i++) {
			    	fitnesses.push_back(ind->getFitnesses()[i]);
		    	}
		    	babies[babies.size()-1]->setFitnesses(fitnesses);
                    }*/
//                    cout << "fitness: " << babies[babies.size() -1]->getFitness() << endl;
                    species->decrementOffspringCount();
                }

                if (a==0)
                {
                    species->updateAgeOfLastImprovement();
                }
            }
            totalIndividualFitness+=ind->getFitness();
        }
        double averageFitness = totalIndividualFitness/generations[onGeneration]->getIndividualCount();
        cout<<"Generation "<<int(onGeneration)<<": "<<"overall_average = "<<averageFitness<<endl;
        cout << "Champion fitness: " << generations[onGeneration]->getIndividual(0)->getFitness() << endl;

        //if(int(generations[onGeneration]->getIndividual(0)->getFitness()) == 25) {
        //    cout << "Optimal solution found in generation " << onGeneration << endl;
        //    exit(0);
        //}

        if (generations[onGeneration]->getIndividual(0)->getUserData())
        {
            cout << "Champion data: " << generations[onGeneration]->getIndividual(0)->getUserData()->summaryToString() << endl;
        }
        cout << "# of Species: " << int(species.size()) << endl;
        cout << "compat threshold: " << Globals::getSingleton()->getParameterValue("CompatibilityThreshold") << endl;

        for (int a=0;a<(int)species.size();a++)
        {
            //cout << "Making babies\n";
            species[a]->makeBabies(babies);
        }
        if ((int)babies.size()!=generations[onGeneration]->getIndividualCount())
        {
            cout << "Population size changed!\n";
            throw CREATE_LOCATEDEXCEPTION_INFO("Population size changed!");
        }

        //cout << "Making new generation\n";
        shared_ptr<GeneticGeneration> newGeneration(generations[onGeneration]->produceNextGeneration(babies,onGeneration+1));
        //cout << "Done Making new generation!\n";

        /*for(int a=0;a<4;a++)
        {
        for(int b=0;b<4;b++)
        {
        cout << babies[a]->getCompatibility(babies[b]) << '\t';
        }

        cout << endl;
        }*/

        generations.push_back(newGeneration);
        onGeneration++;
    }


    void GeneticPopulation::produceNextGenerationShadow() {
	    cout << "In produce next generation (shadow) loop...\n";
        //This clears the link history so future links with the same toNode and fromNode will have different IDs
        Globals::getSingleton()->clearLinkHistory();
        
        
        if(genDataMap.size() == 0) {        
	        ifstream infile;
	        stringstream inputFileName;
//	        inputFileName << "reproduction_data/trimesh_reflect_and_copy_small_displacement_universal_joints_leading_point_trailing_point_step_size_0.001_12500_time_steps_alt_param_mutation_dump_all_" << Globals::getSingleton()->getRandom().getSeed() << ".dat";	        
	        inputFileName << "reproduction_data/trimesh_reflect_and_copy_small_displacement_universal_joints_bh_0.8_low_friction_leading_point_trailing_point_step_size_0.001_12500_time_steps_spacing_0.025_alt_param_mutation_dump_all_" << Globals::getSingleton()->getRandom().getSeed() << ".dat";	        
    		infile.open(inputFileName.str().c_str());
    		string line="test";
    		//istringstream instream; 
    		while (getline(infile,line))
    		{			
				istringstream input(line);
				int genNum;
				GenData genData;
				input >> genNum;
				getline(infile,line);		
				vector<string> mutations;
				boost::split(mutations, line, boost::is_any_of(" "));
				for(int i=0; i<mutations.size(); i++) {
					vector<string> mutation;
					boost::split(mutation, mutations[i], boost::is_any_of("_"));				
					genData.mutations.push_back(make_pair(atoi(mutation[0].c_str()), atoi(mutation[1].c_str())));
				}
				getline(infile,line);		        				
				vector<string> crossovers;
				boost::split(crossovers, line, boost::is_any_of(" "));				
				for(int i=0; i<crossovers.size(); i++) {
					vector<string> crossover;
					boost::split(crossover, crossovers[i], boost::is_any_of("_"));			
					genData.crossovers.push_back(make_pair(make_pair(atoi(crossover[0].c_str()), atoi(crossover[1].c_str())), make_pair(atoi(crossover[2].c_str()), atoi(crossover[3].c_str()))));
				}				
				getline(infile,line);
				vector<string> survivors;
				boost::split(survivors, line, boost::is_any_of(" "));
				for(int i=0; i<survivors.size(); i++) {
					vector<string> survivor;
					boost::split(survivor, survivors[i], boost::is_any_of("_"));				
					genData.survivors.push_back(make_pair(atoi(survivor[0].c_str()), atoi(survivor[1].c_str())));
				}	
				genDataMap[genNum] = genData;
			}
			cout << "done reading\n";
        }

        int numParents = int(generations[onGeneration]->getIndividualCount());

        //This is the new generation
        vector<shared_ptr<GeneticIndividual> > babies;
		GenData genData = genDataMap[onGeneration + 1];   
		//cout <<   genData.survivors.size() << " survivors\n";
		map<shared_ptr<GeneticIndividual>, pair<int,int> > newReproductionStats;
		cout << "getting survivors\n";		
		for(int i=0; i<genData.survivors.size(); i++) {

			vector<shared_ptr<GeneticIndividual> > potentialSurvivors;			
			vector<shared_ptr<GeneticIndividual> > potentialReserveSurvivors;			
			int numMutations = genData.survivors[i].first;
			int numCrossovers = genData.survivors[i].second;	
			//first try to take just "valid morphologies"		
			if (onGeneration == 0) {
				for(int j=0; j<	numParents; j++) {
					potentialReserveSurvivors.push_back(generations[onGeneration]->getIndividual(j));
					if (generations[onGeneration]->getIndividual(j)->getFitness() > 2.0)
						potentialSurvivors.push_back(generations[onGeneration]->getIndividual(j));
				}		
			} else {
				int range = 0;
				while(potentialReserveSurvivors.size() == 0) {
					for(map<shared_ptr<GeneticIndividual>, pair<int,int> >::iterator it = reproductionStats.begin(); it != reproductionStats.end(); it++) {
						if (abs(it->second.first - numMutations) <= range && abs(it->second.second - numCrossovers) <= range) {
							potentialReserveSurvivors.push_back(it->first);					
							if(it->first->getFitness() > 2.0)
								potentialSurvivors.push_back(it->first);
						}
					}
					range++;
				}
			}
			if (potentialSurvivors.size() == 0) {
				//if no valid morphologies, take any matching genome	
				potentialSurvivors = potentialReserveSurvivors;					
				if(potentialSurvivors.size() == 0) {
				//if still 0, something is wrong -- shouldn't happen
					cout << "no matching individuals to survive!\n";
					cout << numMutations << " " << numCrossovers << endl;
					for(map<shared_ptr<GeneticIndividual>, pair<int,int> >::iterator it = reproductionStats.begin(); it != reproductionStats.end(); it++) {
						cout << it->second.first << " " << it->second.second << endl;
					}					
		            exit(-1);
				}
			}
			shared_ptr<GeneticIndividual> ind = potentialSurvivors[Globals::getSingleton()->getRandom().getRandomInt(potentialSurvivors.size())];
			babies.push_back(shared_ptr<GeneticIndividual>(new GeneticIndividual(ind,false)));		
			newReproductionStats[babies[babies.size() - 1]] = make_pair(numMutations,numCrossovers);				
		}  
		cout << "getting mutants\n";		
		for(int i=0; i<genData.mutations.size(); i++) {
			vector<shared_ptr<GeneticIndividual> > potentialMutants;			
			vector<shared_ptr<GeneticIndividual> > potentialReserveMutants;						
			int numMutations = genData.mutations[i].first;
			int numCrossovers = genData.mutations[i].second;			
			if (onGeneration == 0) {
				for(int j=0; j<	numParents; j++) {
					potentialReserveMutants.push_back(generations[onGeneration]->getIndividual(j));
					if (generations[onGeneration]->getIndividual(j)->getFitness() > 2.0)
						potentialMutants.push_back(generations[onGeneration]->getIndividual(j));
				}		
			} else {
				int range = 0;
				while(potentialReserveMutants.size() == 0) {
					for(map<shared_ptr<GeneticIndividual>, pair<int,int> >::iterator it = reproductionStats.begin(); it != reproductionStats.end(); it++) {
						if (abs(it->second.first - numMutations) <= range && abs(it->second.second - numCrossovers) <= range) {
							potentialReserveMutants.push_back(it->first);
							if(it->first->getFitness() > 2.0)
								potentialMutants.push_back(it->first);
						}
					}
					range++;
				}
			}
			if (potentialMutants.size() == 0) {
				//if no valid morphologies, take any matching genome	
				potentialMutants = potentialReserveMutants;					
				if(potentialMutants.size() == 0) {
				//if still 0, something is wrong -- shouldn't happen
					cout << "no matching individuals to mutate!\n";
					cout << numMutations << " " << numCrossovers << endl;
					for(map<shared_ptr<GeneticIndividual>, pair<int,int> >::iterator it = reproductionStats.begin(); it != reproductionStats.end(); it++) {
						cout << it->second.first << " " << it->second.second << endl;
					}					
					exit(-1);
				}
			}			
			shared_ptr<GeneticIndividual> ind = potentialMutants[Globals::getSingleton()->getRandom().getRandomInt(potentialMutants.size())];
			babies.push_back(shared_ptr<GeneticIndividual>(new GeneticIndividual(ind,true)));		
			newReproductionStats[babies[babies.size() - 1]] = make_pair(numMutations+1,numCrossovers);				
		}  
		cout << "getting crossovers\n";		
		for(int i=0; i<genData.crossovers.size(); i++) {
			//search all for valid crossovers
			bool validCrossovers = false;
			int range = 0;
			vector<shared_ptr<GeneticIndividual> > potentialFirstParents;			
			vector<shared_ptr<GeneticIndividual> > potentialSecondParents;	
			vector<shared_ptr<GeneticIndividual> > potentialReserveFirstParents;			
			vector<shared_ptr<GeneticIndividual> > potentialReserveSecondParents;								
			int numMutationsFirstParent = genData.crossovers[i].first.first;
			int numCrossoversFirstParent = genData.crossovers[i].first.second;			
			int numMutationsSecondParent = genData.crossovers[i].second.first;
			int numCrossoversSecondParent = genData.crossovers[i].second.second;									
			while(!validCrossovers) {
				potentialFirstParents.clear();
				potentialSecondParents.clear();
				potentialReserveFirstParents.clear();
				potentialReserveSecondParents.clear();
			
				if (onGeneration == 0) {
					for(int j=0; j<	numParents; j++) {
						potentialReserveFirstParents.push_back(generations[onGeneration]->getIndividual(j));
						potentialReserveSecondParents.push_back(generations[onGeneration]->getIndividual(j));										
						if (generations[onGeneration]->getIndividual(j)->getFitness() > 2.0) {
							potentialFirstParents.push_back(generations[onGeneration]->getIndividual(j));
							potentialSecondParents.push_back(generations[onGeneration]->getIndividual(j));						
						}
					}		
				} else {
					for(map<shared_ptr<GeneticIndividual>, pair<int,int> >::iterator it = reproductionStats.begin(); it != reproductionStats.end(); it++) {
						if (abs(it->second.first - numMutationsFirstParent) <= range && abs(it->second.second - numCrossoversFirstParent) <= range) {
							potentialReserveFirstParents.push_back(it->first);					
							if (it->first->getFitness() > 2.0)
								potentialFirstParents.push_back(it->first);
						}
						if (abs(it->second.first - numMutationsSecondParent) <= range && abs(it->second.second - numCrossoversSecondParent) <= range) {
							potentialReserveSecondParents.push_back(it->first);
							if (it->first->getFitness() > 2.0)
								potentialSecondParents.push_back(it->first);
						}						
					}
				}

				for(int j=0; j<potentialFirstParents.size() && !validCrossovers; j++) {
					for(int k=0; k<potentialSecondParents.size() && !validCrossovers; k++) {
						if((potentialFirstParents[j] != potentialSecondParents[k]) && (potentialFirstParents[j]->getSpeciesID() == potentialSecondParents[k]->getSpeciesID()))
							validCrossovers = true;
					}
				}
				if(!validCrossovers) {// try with reserves
					for(int j=0; j<potentialReserveFirstParents.size() && !validCrossovers; j++) {
						for(int k=0; k<potentialSecondParents.size() && !validCrossovers; k++) {
							if((potentialReserveFirstParents[j] != potentialSecondParents[k]) && (potentialReserveFirstParents[j]->getSpeciesID() == potentialSecondParents[k]->getSpeciesID()))
								validCrossovers = true;
						}
					}
					if(validCrossovers)
						potentialFirstParents = potentialReserveFirstParents;
				}
				if(!validCrossovers) {// try with other reserves
					for(int j=0; j<potentialFirstParents.size() && !validCrossovers; j++) {
						for(int k=0; k<potentialReserveSecondParents.size() && !validCrossovers; k++) {
							if((potentialFirstParents[j] != potentialReserveSecondParents[k]) && (potentialFirstParents[j]->getSpeciesID() == potentialReserveSecondParents[k]->getSpeciesID()))
								validCrossovers = true;
						}
					}
					if(validCrossovers)
						potentialSecondParents = potentialReserveSecondParents;
				}
				if(!validCrossovers) {// try with both reserves
					for(int j=0; j<potentialReserveFirstParents.size() && !validCrossovers; j++) {
						for(int k=0; k<potentialReserveSecondParents.size() && !validCrossovers; k++) {
							if((potentialReserveFirstParents[j] != potentialReserveSecondParents[k]) && (potentialReserveFirstParents[j]->getSpeciesID() == potentialReserveSecondParents[k]->getSpeciesID()))
								validCrossovers = true;
						}
					}
					if(validCrossovers) {
						potentialFirstParents = potentialReserveFirstParents;
						potentialSecondParents = potentialReserveSecondParents;
					}
				}
				
				if(!validCrossovers)
					range++;//extend range and keep searching
			}
			bool allowDifferentSpecies = false;
			
			
			/*
			if(!validCrossovers) {//will have to resort to crossing over between different species
				cout << "ALERT: no valid crossovers within same species, resorting to crossing over among different species\n";
				allowDifferentSpecies = true;
				for(int j=0; j<potentialFirstParents.size() && !validCrossovers; j++) {
					for(int k=0; k<potentialSecondParents.size() && !validCrossovers; k++) {
						if((potentialFirstParents[j] != potentialSecondParents[k]))
							validCrossovers = true;
					}
				}			
			}
			if(!validCrossovers) {// try with reserves
				for(int j=0; j<potentialReserveFirstParents.size() && !validCrossovers; j++) {
					for(int k=0; k<potentialSecondParents.size() && !validCrossovers; k++) {
						if((potentialReserveFirstParents[j] != potentialSecondParents[k]))
							validCrossovers = true;
					}
				}
				if(validCrossovers)
					potentialFirstParents = potentialReserveFirstParents;
			}
			if(!validCrossovers) {// try with other reserves
				for(int j=0; j<potentialFirstParents.size() && !validCrossovers; j++) {
					for(int k=0; k<potentialReserveSecondParents.size() && !validCrossovers; k++) {
						if((potentialFirstParents[j] != potentialReserveSecondParents[k]))
							validCrossovers = true;
					}
				}
				if(validCrossovers)
					potentialSecondParents = potentialReserveSecondParents;
			}
			if(!validCrossovers) {// try with both reserves
				for(int j=0; j<potentialReserveFirstParents.size() && !validCrossovers; j++) {
					for(int k=0; k<potentialReserveSecondParents.size() && !validCrossovers; k++) {
						if((potentialReserveFirstParents[j] != potentialReserveSecondParents[k]))
							validCrossovers = true;
					}
				}
				if(validCrossovers) {
					potentialFirstParents = potentialReserveFirstParents;
					potentialSecondParents = potentialReserveSecondParents;
				}
			}		
			if(!validCrossovers) {//finally give up
				cout << "No valid crossovers!\n";
				cout << numMutationsFirstParent << " " << numCrossoversFirstParent << " " << numMutationsSecondParent << " " << numCrossoversSecondParent << endl;
				cout << potentialFirstParents.size() << "\n";				
				for (int i=0; i<potentialFirstParents.size(); i++)
					cout << potentialFirstParents[i]->getSpeciesID() << " " << reproductionStats[potentialFirstParents[i]].first << " " << reproductionStats[potentialFirstParents[i]].second << endl;
				cout << potentialSecondParents.size() << endl;									
				for (int i=0; i<potentialSecondParents.size(); i++)
					cout << potentialSecondParents[i]->getSpeciesID() << " " << reproductionStats[potentialSecondParents[i]].first << " " << reproductionStats[potentialSecondParents[i]].second << endl;							
				exit(-1);
			}	
			*/
			shared_ptr<GeneticIndividual> ind1;
			shared_ptr<GeneticIndividual> ind2;
			int numTries = 0;
			do {
				ind1 = potentialFirstParents[Globals::getSingleton()->getRandom().getRandomInt(potentialFirstParents.size())];
				ind2 = potentialSecondParents[Globals::getSingleton()->getRandom().getRandomInt(potentialSecondParents.size())];			
				numTries++;								
			} while(numTries < 100000 && (ind1 == ind2 || ((!allowDifferentSpecies) && ind1->getSpeciesID() != ind2->getSpeciesID())));
			if( numTries == 100000 ) {
				cout << "too many tries to crossover!\n";
				cout << numMutationsFirstParent << " " << numCrossoversFirstParent << " " << numMutationsSecondParent << " " << numCrossoversSecondParent << endl;
				cout << potentialFirstParents.size() << "\n";				
				for (int i=0; i<potentialFirstParents.size(); i++)
					cout << potentialFirstParents[i]->getSpeciesID() << " " << reproductionStats[potentialFirstParents[i]].first << " " << reproductionStats[potentialFirstParents[i]].second << endl;
				cout << potentialSecondParents.size() << endl;									
				for (int i=0; i<potentialSecondParents.size(); i++)
					cout << potentialSecondParents[i]->getSpeciesID() << " " << reproductionStats[potentialSecondParents[i]].first << " " << reproductionStats[potentialSecondParents[i]].second << endl;											
				exit(-1);
			}
			
			babies.push_back(shared_ptr<GeneticIndividual>(new GeneticIndividual(ind1,ind2)));		
			newReproductionStats[babies[babies.size() - 1]] = make_pair(max(numMutationsFirstParent,numMutationsSecondParent),max(numCrossoversFirstParent,numCrossoversSecondParent)+1);				
		} 
		cout << "Produced shadow population from generation " << onGeneration << endl;
        //cout << "Making new generation\n";
        shared_ptr<GeneticGeneration> newGeneration(generations[onGeneration]->produceNextGeneration(babies,onGeneration+1));
        //cout << "Done Making new generation!\n";


        generations.push_back(newGeneration);
        onGeneration++;    
	    reproductionStats = newReproductionStats;
    }

    void GeneticPopulation::produceNextGenerationMultiObjective() {
        cout << "In produce next generation (multi-objective) loop...\n";
        //This clears the link history so future links with the same toNode and fromNode will have different IDs
        Globals::getSingleton()->clearLinkHistory();

        vector<shared_ptr<GeneticIndividual> > parents;
        int size = (generations[onGeneration]->getIndividualCount()/2);
        int rank = 0;

        while (parents.size() + ranks[rank].size() <=  size) {
            calculateCrowdingDistances(ranks[rank]);
            parents.insert(parents.end(), ranks[rank].begin(), ranks[rank].end());
            rank += 1;
        }

        calculateCrowdingDistances(ranks[rank]);
        IndividualComparator comparator;
        stable_sort(ranks[rank].begin(), ranks[rank].end(), comparator);
        int n = size - parents.size();
        parents.insert(parents.end(), ranks[rank].begin(), ranks[rank].begin() + n);


        for(int i=0; i<parents.size(); i++) {
            parents[i]->setCanReproduce(true);
        }



        vector<shared_ptr<GeneticIndividual> > babies;
        babies.insert(babies.end(), parents.begin(), parents.end());
        int offspringCount = size;
        if(generations[onGeneration]->getIndividualCount() %2 > 0) //odd pop size, so need one more
        	offspringCount += 1; 
        

        double mutateOnlyProb = Globals::getSingleton()->getParameterValue("MutateOnlyProbability");
        for (int a=0;offspringCount>0;a++)
        {
            if (a>=1000000)
            {
                cout << "Error while making babies\n";
                exit(-1);
            }

            //binary tournament, asexual reproduction
            int choice1 = Globals::getSingleton()->getRandom().getRandomWithinRange(0,parents.size() - 1);
            int choice2 = Globals::getSingleton()->getRandom().getRandomWithinRange(0,parents.size() - 1);
            vector<shared_ptr<GeneticIndividual> > choices;
            choices.push_back(parents[choice1]);
            choices.push_back(parents[choice2]);
            stable_sort(choices.begin(), choices.end(), comparator);

            babies.push_back(shared_ptr<GeneticIndividual>(new GeneticIndividual(choices[0],true)));
            offspringCount--;
        }



        if ((int)babies.size()!=generations[onGeneration]->getIndividualCount())
        {
            cout << "Population size changed! Should be " << generations[onGeneration]->getIndividualCount() << ", but is " << (int)babies.size() << "\n";
            throw CREATE_LOCATEDEXCEPTION_INFO("Population size changed!");
        }

        cout<<"Generation "<<int(onGeneration)<<endl;
        shared_ptr<GeneticGeneration> newGeneration(generations[onGeneration]->produceNextGeneration(babies,onGeneration+1));
        generations.push_back(newGeneration);
        onGeneration++;
        cout << "Done producing\n";


    }

    //double F_MAXES[2][2] = {{0, 50},{0, 0.5}};

    void GeneticPopulation::calculateCrowdingDistances(vector<shared_ptr<GeneticIndividual> > front) {
        if(front.size() == 0)
            return;

        for(int p = 0; p<front.size(); p++) {
            front[p]->setCrowdingDistance(0);
        }

        for(int m=0; m < front[0]->getFitnesses().size(); m++) {
            FitnessComparator comparator(m);
            stable_sort(front.begin(),front.end(),comparator);
            front[0]->setCrowdingDistance(99999999);
            front[front.size() - 1]->setCrowdingDistance( 99999999 );
            for(int i=1; i< front.size() - 1; i++) {
                if(int(front[i]->getCrowdingDistance()) < 99999999) {
                    //front[i]->setCrowdingDistance( front[i]->getCrowdingDistance() + (front[i+1]->getFitnesses()[m] - front[i-1]->getFitnesses()[m])/(F_MAXES[m][1] - F_MAXES[m][0]));
                    front[i]->setCrowdingDistance( front[i]->getCrowdingDistance() + (front[i+1]->getFitnesses()[m] - front[i-1]->getFitnesses()[m])/(front[front.size() - 1]->getFitnesses()[m] - front[0]->getFitnesses()[m]));
                }
            }
        }
    }

    void GeneticPopulation::rankByDominance()
    {
        vector<vector<int> > dominates;
        vector<int> dominationCount;
        vector<int> front;

        ranks.clear();

        cout << "Ranking by dominance *******************************\n";
        int rank = 0;
        for(int i=0; i<1; i++) {
            vector<shared_ptr<GeneticIndividual> > tempRank;
            ranks.push_back(tempRank);
        }

        for (int p=0;p<generations[onGeneration]->getIndividualCount();p++)
        {
            vector<int> temp;
            dominates.push_back(temp);
            dominationCount.push_back(0);

            bool nonDominated = true; //for purposes of Deciding who to back up


            for (int q=0;q<generations[onGeneration]->getIndividualCount();q++)
            {
                if( p != q) {
                    vector<double> fitnessesP = generations[onGeneration]->getIndividual(p)->getFitnesses();
                    vector<double> fitnessesQ = generations[onGeneration]->getIndividual(q)->getFitnesses();

                    if(Globals::getSingleton()->getParameterValue("GenotypicDiversity", 0.0) > 0.5) {
                        bool pDominatesQ = true;
                        bool qDominatesP = true;
                        for(int i=0; i<(fitnessesP.size() - 1); i++)
                        {
                            long fitP, fitQ;
                            if( fabs(fitnessesP[i] - fitnessesQ[i]) < 2 && fitnessesP[i] < 1000 && fitnessesQ[i] < 1000) {
                                fitP = (long) (fitnessesP[i] * long(1000000));
                                fitQ = (long) (fitnessesQ[i] * long(1000000));
                            } else {
                                fitP = (long) fitnessesP[i];
                                fitQ = (long) fitnessesQ[i];
                            }
                            if(fitP > fitQ)
                                qDominatesP = false;
                            else if(fitQ > fitP)
                                pDominatesQ = false;
                        }
                        if((!pDominatesQ) && qDominatesP) {
                            nonDominated = false;
                        }
                    }
                    bool pDominatesQ = true;
                    bool qDominatesP = true;


                    for(int i=0; i<fitnessesP.size(); i++)
                    {
                        long fitP, fitQ;
                        if( fabs(fitnessesP[i] - fitnessesQ[i]) < 2 && fitnessesP[i] < 1000 && fitnessesQ[i] < 1000) {
                            fitP = (long) (fitnessesP[i] * long(1000000));
                            fitQ = (long) (fitnessesQ[i] * long(1000000));
                        } else {
                            fitP = (long) fitnessesP[i];
                            fitQ = (long) fitnessesQ[i];
                        }


                        if(fitP > fitQ)
                            qDominatesP = false;
                        else if(fitQ > fitP)
                            pDominatesQ = false;
                    }
                    if(pDominatesQ) {
                        dominates[p].push_back(q);
                    } else if(qDominatesP) {
                        dominationCount[p]++;
                    }
                }
            }

            if(dominationCount[p] == 0) {
                generations[onGeneration]->getIndividual(p)->setRank(rank);
                ranks[0].push_back(generations[onGeneration]->getIndividual(p));
                front.push_back(p);
                if(Globals::getSingleton()->getParameterValue("GenotypicDiversity", 0.0) > 0.5) {
                    if(nonDominated)
                        generations[onGeneration]->addNonDominated(generations[onGeneration]->getIndividual(p));
                } else {
                    generations[onGeneration]->addNonDominated(generations[onGeneration]->getIndividual(p));
                }
            }
        }


        //bool done = false;
        for(int j=0; j < 2; j++) {
            cout << "NONDOMINATED:";

            for(int i=0; i < front.size(); i++) {
                vector<double> fits = generations[onGeneration]->getIndividual(front[i])->getFitnesses();
                cout << " " << fits[j];
                //if(j == 0 && int(fits[0] * fits[1]) == 25)
                //    done = true;
            }
            cout << endl;
        }
        //if(done) {
        //    cout << "Optimal solution found in generation " << onGeneration << endl;
        //    exit(0);
        //}



        while(front.size() > 0) {
            rank++;
            vector<shared_ptr<GeneticIndividual> > tempRank;
            ranks.push_back(tempRank);
            vector<int> newFront;
            for (int i=0; i < front.size(); i++) {
                int p = front[i];
                for(int j = 0; j < dominates[p].size(); j++) {
                    int q = dominates[p][j];
                    dominationCount[q]--;
                    if(dominationCount[q] == 0) {
                        generations[onGeneration]->getIndividual(q)->setRank(rank);
                        ranks[rank].push_back(generations[onGeneration]->getIndividual(q));
                        newFront.push_back(q);
                    }
                }
            }
            front.clear();
            front = newFront;
        }
    }

    void GeneticPopulation::dump(string filename,bool includeGenes,bool doGZ)
    {

        cout << "dump -- filename: " << filename << " includeGenes: " << includeGenes << " doGZ: " << doGZ << endl;
        stringstream ss;
        ss << (generations.size()-1);
        TiXmlDocument doc( filename + string("-") + ss.str() + string(".backup.xml") );
        if (generations.size())
        {

            TiXmlElement *generationElementPtr = new TiXmlElement(generations[generations.size()-1]->getTypeName());
            generations[generations.size()-1]->dump(generationElementPtr,includeGenes);
            doc.LinkEndChild(generationElementPtr);
        }

        if (doGZ)
        {
            doc.SaveFileGZ();
        }
        else
        {
            doc.SaveFile();
        }

        /*

        TiXmlDocument doc( filename );

        TiXmlElement *root = new TiXmlElement("Genetics");

        Globals::getSingleton()->dump(root);

        doc.LinkEndChild(root);

        for (int a=0;a<(int)generations.size();a++)
        {

            TiXmlElement *generationElementPtr = new TiXmlElement(generations[a]->getTypeName());

            root->LinkEndChild(generationElementPtr);

            generations[a]->dump(generationElementPtr,includeGenes);
        }

        if (doGZ)
        {
            doc.SaveFileGZ();
        }
        else
        {
            doc.SaveFile();
        }
        */
    }

    void GeneticPopulation::dumpReproducedFromPreviousGeneration(string filename, bool includeGenes, bool doGZ) {
        cout << "dumpReproducedFromPreviousGeneration -- filename: " << filename << " includeGenes: " << includeGenes << " doGZ: " << doGZ << endl;

        if(generations.size() <= 1) {
            cout << "Error!  No previous generation to dump!\n";
            throw CREATE_LOCATEDEXCEPTION_INFO("Error!  No previous generation to dump!");
        }

        stringstream ss;
        ss << (generations.size()-2);
        TiXmlDocument doc( filename + string("-") + ss.str() + string(".backup.xml") );
        if (generations.size())
        {

            TiXmlElement *generationElementPtr = new TiXmlElement(generations[generations.size()-2]->getTypeName());
            //generations[generations.size()-1]->dumpBest(generationElementPtr,includeGenes);
            generations[generations.size()-2]->dumpReproduced(generationElementPtr,includeGenes);
            doc.LinkEndChild(generationElementPtr);
        }

        if (doGZ)
        {
            doc.SaveFileGZ();
        }
        else
        {
            doc.SaveFile();
        }


    }


    void GeneticPopulation::dumpBest(string filename,bool includeGenes,bool doGZ)
    {
        cout << "dumpBest -- filename: " << filename << " includeGenes: " << includeGenes << " doGZ: " << doGZ << endl;

        if(true) {//Globals::getSingleton()->getParameterValue("MultiObjective", 0.0) > 0.5) {
                stringstream ss;
                ss << (generations.size()-1);
                TiXmlDocument doc( filename + string("-") + ss.str() + string(".backup.xml") );
                if (generations.size())
                {

                    TiXmlElement *generationElementPtr = new TiXmlElement(generations[generations.size()-1]->getTypeName());
                    generations[generations.size()-1]->dumpBest(generationElementPtr,includeGenes);
                    doc.LinkEndChild(generationElementPtr);
                }

                if (doGZ)
                {
                    doc.SaveFileGZ();
                }
                else
                {
                    doc.SaveFile();
                }


            /*
            string existingFile = filename;
            if(doGZ)
            {
                existingFile = filename + ".gz";
            }

            ifstream *ifile = new ifstream(existingFile.c_str());
            if(ifile->good())
            {
                cout << "file exists!" << endl;
                TiXmlDocument doc( existingFile );

                bool loadStatus;

                if (doGZ)
                {
                    loadStatus = doc.LoadFileGZ();
                }
                else
                {
                    loadStatus = doc.LoadFile();
                }

                if (!loadStatus)
                {
                    throw CREATE_LOCATEDEXCEPTION_INFO("Error trying to load the XML file!");
                }


                TiXmlElement *root = doc.FirstChildElement();


                if (generations.size())
                {
                    //dump best guys from latest generation
                    TiXmlElement *generationElementPtr = new TiXmlElement(generations[generations.size()-1]->getTypeName());
                    generations[generations.size()-1]->dumpBest(generationElementPtr,includeGenes);
                    root->LinkEndChild(generationElementPtr);
                }

                if (doGZ)
                {
                    doc.SaveFileGZ(existingFile.c_str());
                }
                else
                {
                    doc.SaveFile();
                }
            }
            else
            {
                cout << "file does NOT exist!" << endl;

                TiXmlDocument doc( filename );

                TiXmlElement *root = new TiXmlElement("Genetics");

                Globals::getSingleton()->dump(root);

                doc.LinkEndChild(root);

                if (generations.size())
                {
                    //Always dump everyone from the final generation

                    //changing to only ever dump best

                    TiXmlElement *generationElementPtr = new TiXmlElement(generations[generations.size()-1]->getTypeName());
                    generations[generations.size()-1]->dumpBest(generationElementPtr,includeGenes);
                    root->LinkEndChild(generationElementPtr);
                }

                if (doGZ)
                {
                    doc.SaveFileGZ();
                }
                else
                {
                    doc.SaveFile();
                }
            }
            ifile->close();
            delete ifile;
            */
        } else {
            string existingFile = filename;
            if(doGZ)
            {
                existingFile = filename + ".gz";
            }

            ifstream *ifile = new ifstream(existingFile.c_str());
            if(ifile->good())
            {
                cout << "file exists!" << endl;
                TiXmlDocument doc( existingFile );

                bool loadStatus;

                if (doGZ)
                {
                    loadStatus = doc.LoadFileGZ();
                }
                else
                {
                    loadStatus = doc.LoadFile();
                }

                if (!loadStatus)
                {
                    throw CREATE_LOCATEDEXCEPTION_INFO("Error trying to load the XML file!");
                }


                TiXmlElement *root = doc.FirstChildElement();


                if (generations.size())
                {
                    //dump best guys from latest generation
                    TiXmlElement *generationElementPtr = new TiXmlElement(generations[generations.size()-1]->getTypeName());
                    generations[generations.size()-1]->dumpBest(generationElementPtr,includeGenes);
                    root->LinkEndChild(generationElementPtr);
                }

                if (doGZ)
                {
                    doc.SaveFileGZ(existingFile.c_str());
                }
                else
                {
                    doc.SaveFile();
                }
            }
            else
            {
                cout << "file does NOT exist!" << endl;

                TiXmlDocument doc( filename );

                TiXmlElement *root = new TiXmlElement("Genetics");

                Globals::getSingleton()->dump(root);

                doc.LinkEndChild(root);

                if (generations.size())
                {
                    //Always dump everyone from the final generation

                    //changing to only ever dump best

                    TiXmlElement *generationElementPtr = new TiXmlElement(generations[generations.size()-1]->getTypeName());
                    generations[generations.size()-1]->dumpBest(generationElementPtr,includeGenes);
                    root->LinkEndChild(generationElementPtr);
                }

                if (doGZ)
                {
                    doc.SaveFileGZ();
                }
                else
                {
                    doc.SaveFile();
                }
            }
            ifile->close();
            delete ifile;
/*
            TiXmlDocument doc( filename );

            TiXmlElement *root = new TiXmlElement("Genetics");

            Globals::getSingleton()->dump(root);

            doc.LinkEndChild(root);

            for (int a=0;a<int(generations.size())-1;a++)
            {

                TiXmlElement *generationElementPtr = new TiXmlElement(generations[a]->getTypeName());

                root->LinkEndChild(generationElementPtr);

                generations[a]->dumpBest(generationElementPtr,includeGenes);
            }

            if (generations.size())
            {
                //Always dump everyone from the final generation
                TiXmlElement *generationElementPtr = new TiXmlElement(generations[generations.size()-1]->getTypeName());
                generations[generations.size()-1]->dump(generationElementPtr,includeGenes);
                root->LinkEndChild(generationElementPtr);
            }

            if (doGZ)
            {
                doc.SaveFileGZ();
            }
            else
            {
                doc.SaveFile();
            }
*/

        }
    }

    void GeneticPopulation::cleanupOld(int generationSkip)
    {
    	cout << "Cleaning up... on generation " << onGeneration << endl;
        for (int a=0;a<onGeneration;a++)
        {
            if ( (a%generationSkip) == 0 )
                continue;

            generations[a]->cleanup();
        }
    }
}

