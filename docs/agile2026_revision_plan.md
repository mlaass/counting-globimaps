# AGILE 2026 Revision Plan

**Paper:** CascadeCBF: Probabilistic Counting for Sparse Spatial Point Clouds
**Deadline:** 10 April 2026
**Registration:** Presenting author by 1 May 2026

Reviews: 5 (accept), 5 (accept), 3 (borderline), 5 (accept), 5 (accept)

---

## Abstract & Introduction

- [ ] Add 1-2 quantitative results to the abstract (R2) -- e.g., "<0.01% error at k>=4", "1.3-1.6x throughput"
- [ ] Consider replacing "power-law" with "long-tail" or note applicability to long-tail distributions (R5)
- [ ] Sentence 2: clarify that cardinality estimation is not a set operation; remove "per location" if referencing trajectories (R5)
- [ ] Sentence 3: remove "Thus," (R5)
- [ ] Make first paragraph more succinct (R5)

## Related Work & Theory

- [ ] Discuss how CascadeCBF differs from existing multi-layer/hierarchical CBFs -- especially Multilayer Compressed CBFs (R3)
- [ ] Expand theoretical analysis of the approach (R3)
- [ ] Strengthen discussion of practical deployment scenarios and trade-offs vs existing approaches (R1)

## GIS Relevance

- [ ] Highlight concrete GIS use cases and spatial analysis scenarios more explicitly (R1)
- [ ] Expand explanation of spatial operations' relevance to typical GIS workflows (R1)

## Evaluation Section

- [ ] Restructure: clearly separate synthetic vs real-world experiments (R5 suggests subsections)
- [ ] Section 5.1: add missing insertion time numbers (R5)
- [ ] Explain why 0% error in time-series experiments -- parameter settings or dataset characteristics? (R3)
- [ ] Clarify: is "independent hashing" the same as "hashing trick"? Why used only for Fig 3? (R5)
- [ ] Clarify query types: simple element lookups vs complex queries (R5)
- [ ] Figure 2: explain why standard mode / base CascadeCBF (without MI) is missing (R5)
- [ ] Explain why Union and Deletion are not evaluated; justify alignment with geo-domain use cases (R5)
- [ ] Provide suggested design/parametric settings for general geo data (R5)

## Figures & Naming

- [ ] Unify acronyms: pick one of CascadeCBF / CCBF / CCBF (MI+IH) and use consistently in text and figures (R5)
- [ ] Improve dense figure captions with clearer explanations (R1)
- [ ] Simplify long, technically dense sentences throughout (R1)

## DASA Section & Reproducibility

- [ ] Ensure code and data are available in DASA section per AGILE requirements (editor)
- [ ] Add persistent links/footnotes for GDELT and COVID-19 data sources with versions (R5)
- [ ] Add LICENSE file to code repository (R5)
- [ ] Clarify implementation details: memory allocation, layer sizing, parameter choices (R2 data, R1)

## Nice-to-Have (if time permits)

- [ ] Add Union/Deletion evaluation results in supplementary materials (R5)
- [ ] Consider Python wrapper/package mention in future work (R5)
- [ ] Add more critical discussion of practical deployment trade-offs (R1)

## De-anonymize for Camera-Ready

- [ ] Restore author names, affiliations, correspondence
- [ ] Restore inline citations that were anonymized (search for "CAMERA-READY" comments in tex)
- [ ] Update anonymous repo URL to final public repository
